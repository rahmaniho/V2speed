package app.v2speed.android.data

import android.net.Uri
import android.util.Base64
import org.json.JSONArray
import org.json.JSONObject
import java.net.URLDecoder
import java.nio.charset.StandardCharsets
import java.security.MessageDigest

/** A node exists only in memory; the full credential-bearing URI is never persisted or logged. */
data class ServerNode(
    val id: String,
    val name: String,
    val endpoint: String,
    val protocol: String,
    val sourceId: String,
    val sourceName: String,
    val uri: String
)

object SubscriptionParser {
    private val schemes = setOf("vless", "vmess", "trojan", "ss")

    fun parseFeed(sourceId: String, sourceName: String, body: String): List<ServerNode> {
        val text = decodeFeed(body)
        val seen = HashSet<String>()
        return text.lineSequence().mapNotNull { raw ->
            val line = raw.trim().removePrefix("\uFEFF")
            if (!line.contains("://")) return@mapNotNull null
            parseNode(sourceId, sourceName, line)
        }.filter { seen.add(it.id) }.toList()
    }

    private fun decodeFeed(body: String): String {
        val text = body.trim().removePrefix("\uFEFF")
        if (text.lineSequence().any { line -> schemes.any { line.trim().startsWith("$it://", true) } }) return text
        val compact = text.filterNot(Char::isWhitespace)
        val padded = compact + "=".repeat((4 - compact.length % 4) % 4)
        return try {
            String(decodeBase64(padded) ?: return text, StandardCharsets.UTF_8)
                .takeIf { decoded -> decoded.lineSequence().any { line -> schemes.any { line.trim().startsWith("$it://", true) } } }
                ?: text
        } catch (_: IllegalArgumentException) {
            text
        }
    }

    fun parseNode(sourceId: String, sourceName: String, input: String): ServerNode? {
        val uriText = input.trim()
        val scheme = uriText.substringBefore("://", "").lowercase()
        if (scheme !in schemes) return null
        val display = when (scheme) {
            "vmess" -> parseVmess(uriText) ?: return null
            "ss" -> parseShadowsocks(uriText) ?: return null
            else -> parseStandard(uriText, scheme) ?: return null
        }
        return ServerNode(
            id = digest("$sourceId\u0000$uriText"),
            name = display.name.ifBlank { display.endpoint },
            endpoint = display.endpoint,
            protocol = if (scheme == "ss") "shadowsocks" else scheme,
            sourceId = sourceId,
            sourceName = sourceName,
            uri = uriText
        )
    }

    fun toXrayConfig(uriText: String): String {
        val scheme = uriText.substringBefore("://", "").lowercase()
        require(scheme in schemes) { "پروتکل پشتیبانی نمی‌شود." }
        val parsed = when (scheme) {
            "vmess" -> parseVmess(uriText) ?: error("لینک VMess نامعتبر است.")
            "ss" -> parseShadowsocks(uriText) ?: error("لینک Shadowsocks نامعتبر است.")
            else -> parseStandard(uriText, scheme) ?: error("لینک پیکربندی نامعتبر است.")
        }
        val outbound = buildOutbound(parsed, scheme)
        val inboundSettings = JSONObject()
            .put("name", "v2speed-tun")
            .put("mtu", 1500)
            .put("gateway", JSONArray().put("10.88.0.1/30").put("fd88::1/126"))
            .put("dns", JSONArray().put("1.1.1.1").put("2606:4700:4700::1111"))
        return JSONObject()
            .put("log", JSONObject().put("loglevel", "warning"))
            .put("inbounds", JSONArray().put(JSONObject()
                .put("tag", "v2speed-tun")
                .put("protocol", "tun")
                .put("settings", inboundSettings)))
            .put("outbounds", JSONArray()
                .put(outbound)
                .put(JSONObject().put("tag", "direct").put("protocol", "freedom"))
                .put(JSONObject().put("tag", "block").put("protocol", "blackhole")))
            .toString()
    }

    private data class Link(
        val name: String,
        val endpoint: String,
        val host: String,
        val port: Int,
        val credential: String,
        val query: Map<String, String>,
        val vmess: JSONObject? = null,
        val ssMethod: String? = null
    )

    private fun parseStandard(input: String, scheme: String): Link? {
        return try {
            val uri = Uri.parse(input)
            val host = uri.host?.takeIf(String::isNotBlank) ?: return null
            val port = uri.port.takeIf { it in 1..65535 } ?: return null
            val credential = uri.encodedUserInfo?.let(Uri::decode)?.takeIf(String::isNotBlank) ?: return null
            val query = uri.queryParameterNames.associateWith { key -> uri.getQueryParameter(key).orEmpty() }
            if (scheme == "ss" && query["plugin"].orEmpty().isNotBlank()) return null
            Link(decodeLabel(uri.fragment).ifBlank { host }, "$host:$port", host, port, credential, query)
        } catch (_: RuntimeException) { null }
    }

    private fun parseVmess(input: String): Link? {
        return try {
            val payload = input.substringAfter("vmess://").substringBefore('#')
            val padded = payload + "=".repeat((4 - payload.length % 4) % 4)
            val data = JSONObject(String(decodeBase64(padded) ?: return null, StandardCharsets.UTF_8))
            val host = data.optString("add").takeIf(String::isNotBlank) ?: return null
            val port = data.optString("port").toIntOrNull()?.takeIf { it in 1..65535 } ?: return null
            val id = data.optString("id").takeIf(String::isNotBlank) ?: return null
            val query = linkedMapOf(
                "type" to data.optString("net", "tcp"),
                "security" to if (data.optString("tls").isNotBlank() && data.optString("tls") != "none") "tls" else "none",
                "sni" to data.optString("sni"), "host" to data.optString("host"),
                "path" to data.optString("path"), "fp" to data.optString("fp"), "alpn" to data.optString("alpn")
            ).filterValues(String::isNotBlank)
            Link(data.optString("ps").takeIf(String::isNotBlank) ?: host, "$host:$port", host, port, id, query, data)
        } catch (_: Exception) { null }
    }

    private fun parseShadowsocks(input: String): Link? {
        return try {
            val noFragment = input.substringAfter("ss://").substringBefore('#')
            val split = noFragment.lastIndexOf('@')
            val methodPassword: String
            val endpoint: String
            if (split >= 0) {
                methodPassword = decodeBase64Text(noFragment.substring(0, split)) ?: noFragment.substring(0, split)
                endpoint = noFragment.substring(split + 1)
            } else {
                val decoded = decodeBase64Text(noFragment) ?: return null
                val at = decoded.lastIndexOf('@')
                if (at < 0) return null
                methodPassword = decoded.substring(0, at)
                endpoint = decoded.substring(at + 1)
            }
            val endpointUri = Uri.parse("ss://$endpoint")
            val host = endpointUri.host?.takeIf(String::isNotBlank) ?: return null
            val port = endpointUri.port.takeIf { it in 1..65535 } ?: return null
            val colon = methodPassword.indexOf(':')
            if (colon <= 0 || colon == methodPassword.lastIndex) return null
            val method = methodPassword.substring(0, colon)
            val password = methodPassword.substring(colon + 1)
            Link(host, "$host:$port", host, port, password, mapOf("method" to method), ssMethod = method)
        } catch (_: RuntimeException) { null }
    }

    private fun buildOutbound(link: Link, scheme: String): JSONObject {
        val outbound = JSONObject().put("tag", "proxy").put("protocol", if (scheme == "ss") "shadowsocks" else scheme)
        val server = JSONObject().put("address", link.host).put("port", link.port)
        when (scheme) {
            "vless" -> {
                val user = JSONObject().put("id", link.credential).put("encryption", link.query["encryption"] ?: "none")
                link.query["flow"]?.takeIf(String::isNotBlank)?.let { user.put("flow", it) }
                outbound.put("settings", JSONObject().put("vnext", JSONArray().put(server.put("users", JSONArray().put(user)))))
            }
            "vmess" -> {
                val data = link.vmess ?: error("VMess data missing")
                val user = JSONObject()
                    .put("id", link.credential)
                    .put("alterId", data.optInt("aid", 0))
                    .put("security", data.optString("scy").takeIf(String::isNotBlank) ?: "auto")
                    .put("level", 0)
                outbound.put("settings", JSONObject().put("vnext", JSONArray().put(server.put("users", JSONArray().put(user)))))
            }
            "trojan" -> outbound.put("settings", JSONObject().put("servers", JSONArray().put(server.put("password", link.credential).put("level", 0))))
            "ss" -> outbound.put("settings", JSONObject().put("servers", JSONArray().put(server
                .put("method", link.ssMethod ?: link.query["method"] ?: error("Shadowsocks method missing"))
                .put("password", link.credential)
                .put("level", 0))))
        }
        val type = link.query["type"]?.lowercase()?.ifBlank { "tcp" } ?: "tcp"
        require(type in setOf("tcp", "ws", "grpc", "httpupgrade")) { "نوع انتقال $type هنوز پشتیبانی نمی‌شود." }
        val security = link.query["security"]?.lowercase()?.ifBlank { "none" } ?: "none"
        require(security in setOf("none", "tls", "reality")) { "تنظیم امنیتی پشتیبانی نمی‌شود." }
        val stream = JSONObject().put("network", type).put("security", security)
        if (security == "tls") {
            val tls = JSONObject().put("allowInsecure", false)
            link.query["sni"]?.takeIf(String::isNotBlank)?.let { tls.put("serverName", it) }
            link.query["fp"]?.takeIf(String::isNotBlank)?.let { tls.put("fingerprint", it) }
            link.query["alpn"]?.takeIf(String::isNotBlank)?.let { tls.put("alpn", JSONArray(it.split(',').map(String::trim))) }
            stream.put("tlsSettings", tls)
        } else if (security == "reality") {
            val reality = JSONObject().put("serverName", link.query["sni"] ?: link.host)
                .put("fingerprint", link.query["fp"] ?: "chrome")
            link.query["pbk"]?.takeIf(String::isNotBlank)?.let { reality.put("publicKey", it) }
                ?: error("Reality public key is missing")
            link.query["sid"]?.takeIf(String::isNotBlank)?.let { reality.put("shortId", it) }
            link.query["spx"]?.takeIf(String::isNotBlank)?.let { reality.put("spiderX", it) }
            stream.put("realitySettings", reality)
        }
        when (type) {
            "ws" -> {
                val ws = JSONObject().put("path", link.query["path"] ?: "/")
                link.query["host"]?.takeIf(String::isNotBlank)?.let { ws.put("headers", JSONObject().put("Host", it)) }
                stream.put("wsSettings", ws)
            }
            "grpc" -> {
                val grpc = JSONObject().put("serviceName", link.query["serviceName"] ?: link.query["path"] ?: "")
                link.query["mode"]?.takeIf(String::isNotBlank)?.let { grpc.put("multiMode", it == "multi") }
                stream.put("grpcSettings", grpc)
            }
            "httpupgrade" -> stream.put("httpupgradeSettings", JSONObject()
                .put("path", link.query["path"] ?: "/")
                .put("host", link.query["host"] ?: ""))
        }
        outbound.put("streamSettings", stream)
        return outbound
    }

    private fun decodeBase64Text(value: String): String? {
        return try {
            val compact = value.trim()
            val padded = compact + "=".repeat((4 - compact.length % 4) % 4)
            val bytes = decodeBase64(padded) ?: return null
            String(bytes, StandardCharsets.UTF_8)
        } catch (_: RuntimeException) { null }
    }

    private fun decodeBase64(value: String): ByteArray? {
        val flags = listOf(Base64.DEFAULT, Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING)
        return flags.firstNotNullOfOrNull { mode -> runCatching { Base64.decode(value, mode) }.getOrNull() }
    }

    private fun decodeLabel(value: String?): String = try {
        URLDecoder.decode(value.orEmpty().replace("+", "%2B"), "UTF-8").trim()
    } catch (_: IllegalArgumentException) { value.orEmpty().trim() }

    private fun digest(value: String): String = MessageDigest.getInstance("SHA-256")
        .digest(value.toByteArray(StandardCharsets.UTF_8)).take(8).joinToString("") { "%02x".format(it) }
}
