package app.v2speed.android.data

import android.util.Base64
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SubscriptionParserTest {
    @Test
    fun parsesVlessAndBuildsTlsWebsocketXrayConfig() {
        val link = "vless://123e4567-e89b-12d3-a456-426614174000@edge.example:443?encryption=none&security=tls&type=ws&sni=cdn.example&host=cdn.example&path=%2Fws#Berlin"
        val node = SubscriptionParser.parseNode("iboxz", "source", link)
        assertNotNull(node)
        assertEquals("Berlin", node!!.name)
        assertEquals("edge.example:443", node.endpoint)
        val config = JSONObject(SubscriptionParser.toXrayConfig(link))
        val inbound = config.getJSONArray("inbounds").getJSONObject(0)
        assertEquals("tun", inbound.getString("protocol"))
        val proxy = config.getJSONArray("outbounds").getJSONObject(0)
        assertEquals("vless", proxy.getString("protocol"))
        assertEquals("tls", proxy.getJSONObject("streamSettings").getString("security"))
        assertFalse(proxy.getJSONObject("streamSettings").getJSONObject("tlsSettings").getBoolean("allowInsecure"))
        assertEquals("/ws", proxy.getJSONObject("streamSettings").getJSONObject("wsSettings").getString("path"))
    }

    @Test
    fun parsesVmessJsonAndBuildsOutbound() {
        val vmess = """{"v":"2","ps":"Tokyo","add":"vmess.example","port":"8443","id":"123e4567-e89b-12d3-a456-426614174000","aid":"0","scy":"auto","net":"tcp","tls":"tls","sni":"vmess.example"}"""
        val encoded = Base64.encodeToString(vmess.toByteArray(), Base64.NO_WRAP)
        val config = JSONObject(SubscriptionParser.toXrayConfig("vmess://$encoded"))
        val proxy = config.getJSONArray("outbounds").getJSONObject(0)
        assertEquals("vmess", proxy.getString("protocol"))
        assertEquals("vmess.example", proxy.getJSONObject("settings").getJSONArray("vnext").getJSONObject(0).getString("address"))
    }

    @Test
    fun parsesTrojanAndShadowsocksAndRejectsUnknownOrMalformedLinks() {
        val trojan = "trojan://secret@trojan.example:443?security=tls&sni=trojan.example#Fast"
        assertEquals("trojan", SubscriptionParser.parseNode("thomas", "source", trojan)?.protocol)
        val auth = Base64.encodeToString("chacha20-ietf-poly1305:secret".toByteArray(), Base64.NO_WRAP)
        val shadowsocks = "ss://$auth@ss.example:8388#SS"
        assertEquals("shadowsocks", SubscriptionParser.parseNode("thomas", "source", shadowsocks)?.protocol)
        assertNull(SubscriptionParser.parseNode("thomas", "source", "http://example.invalid"))
        assertNull(SubscriptionParser.parseNode("thomas", "source", "vmess://broken"))
    }

    @Test
    fun decodesBase64FeedAndDeduplicatesOnlyWithinTheFeed() {
        val line = "vless://123e4567-e89b-12d3-a456-426614174000@edge.example:443#same"
        val encoded = Base64.encodeToString("$line\n$line".toByteArray(), Base64.NO_WRAP)
        val nodes = SubscriptionParser.parseFeed("iboxz", "source", encoded)
        assertEquals(1, nodes.size)
        assertTrue(nodes.single().id.matches(Regex("[a-f0-9]{16}")))
    }
}
