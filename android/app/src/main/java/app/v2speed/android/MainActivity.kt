package app.v2speed.android

import android.Manifest
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.VpnService
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import app.v2speed.android.data.ServerNode
import app.v2speed.android.data.SubscriptionParser
import app.v2speed.android.vpn.V2SpeedVpnService
import java.io.ByteArrayOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val io = Executors.newSingleThreadExecutor()
    private val nodes = mutableListOf<ServerNode>()
    private var selected: ServerNode? = null
    private lateinit var status: TextView
    private lateinit var nodeList: LinearLayout
    private lateinit var connectButton: Button
    private var connected = false
    private var receiverRegistered = false

    private val stateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val state = intent?.getStringExtra(V2SpeedVpnService.EXTRA_STATE) ?: return
            val message = intent.getStringExtra(V2SpeedVpnService.EXTRA_MESSAGE).orEmpty()
            connected = state == V2SpeedVpnService.STATE_CONNECTED || state == V2SpeedVpnService.STATE_CONNECTING
            status.text = message
            status.setTextColor(if (state == V2SpeedVpnService.STATE_ERROR) 0xffff9898.toInt() else 0xffc8ef9a.toInt())
            connectButton.text = if (connected) "توقف VPN" else "اتصال به سرور انتخابی"
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
        loadFeeds()
    }

    @Suppress("DEPRECATION")
    override fun onStart() {
        super.onStart()
        val filter = IntentFilter(V2SpeedVpnService.ACTION_STATE)
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(stateReceiver, filter, RECEIVER_NOT_EXPORTED)
        else registerReceiver(stateReceiver, filter)
        receiverRegistered = true
    }

    override fun onStop() {
        if (receiverRegistered) unregisterReceiver(stateReceiver)
        receiverRegistered = false
        super.onStop()
    }

    private fun buildUi() {
        val background = 0xff080d18.toInt()
        val surface = 0xff111a2b.toInt()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(14), dp(18), dp(16))
            setBackgroundColor(background)
            layoutDirection = View.LAYOUT_DIRECTION_RTL
        }
        val title = TextView(this).apply {
            text = "V2SPEED  ·  کاتالوگ VPN"
            textSize = 22f
            setTextColor(0xff9fff00.toInt())
            setPadding(0, dp(6), 0, dp(8))
        }
        val disclaimer = TextView(this).apply {
            text = "اتصال واقعی از راه Android VPN؛ کیفیت و دسترسی هر سرور شخص ثالث تضمین‌شده نیست. سرعت‌سنج خودکار در این نسخه وجود ندارد."
            textSize = 13f
            setTextColor(0xffcbd7e3.toInt())
            setPadding(0, 0, 0, dp(12))
        }
        status = TextView(this).apply {
            text = "برای بارگیری فهرست، دکمهٔ تازه‌سازی را بزنید."
            textSize = 14f
            setTextColor(0xffcbd7e3.toInt())
            setPadding(dp(12), dp(12), dp(12), dp(12))
            setBackgroundColor(surface)
        }
        val toolbar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val refresh = Button(this).apply {
            text = "بارگیری سه منبع"
            setOnClickListener { loadFeeds() }
        }
        connectButton = Button(this).apply {
            text = "اتصال به سرور انتخابی"
            isEnabled = false
            setOnClickListener { onConnectClicked() }
        }
        toolbar.addView(refresh, LinearLayout.LayoutParams(0, dp(54), 1f))
        toolbar.addView(connectButton, LinearLayout.LayoutParams(0, dp(54), 1f))
        val scroll = ScrollView(this)
        nodeList = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(8), 0, dp(8))
        }
        scroll.addView(nodeList)
        root.addView(title)
        root.addView(disclaimer)
        root.addView(status)
        root.addView(toolbar)
        root.addView(scroll, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        setContentView(root)
    }

    private fun loadFeeds() {
        status.text = "در حال دریافت منابع ثابت اشتراک…"
        nodeList.removeAllViews()
        io.execute {
            val loaded = mutableListOf<ServerNode>()
            val failures = mutableListOf<String>()
            for (source in SOURCES) {
                try {
                    val response = downloadText(source.url)
                    loaded += SubscriptionParser.parseFeed(source.id, source.name, response).take(MAX_NODES_PER_SOURCE)
                } catch (_: Exception) {
                    failures += source.name
                }
            }
            runOnUiThread {
                nodes.clear()
                nodes += loaded
                selected = null
                connectButton.isEnabled = false
                renderNodes()
                status.text = when {
                    loaded.isEmpty() -> "هیچ منبعی در دسترس نیست؛ اتصال انجام نشد."
                    failures.isNotEmpty() -> "${loaded.size} سرور بارگیری شد؛ بعضی منابع در دسترس نبودند: ${failures.joinToString()}"
                    else -> "${loaded.size} سرور از سه منبع بارگیری شد. این شمارش آزمون سرعت یا امنیت نیست."
                }
            }
        }
    }

    private fun renderNodes() {
        nodeList.removeAllViews()
        nodes.forEach { node ->
            val row = TextView(this).apply {
                text = "${node.name}\n${node.endpoint}  ·  ${node.protocol.uppercase()}  ·  ${node.sourceName}"
                textSize = 14f
                setTextColor(if (selected?.id == node.id) 0xff9fff00.toInt() else 0xfff5f7fa.toInt())
                setBackgroundColor(if (selected?.id == node.id) 0xff22351e.toInt() else 0xff111a2b.toInt())
                setPadding(dp(13), dp(12), dp(13), dp(12))
                contentDescription = "${node.name}, ${node.endpoint}, ${node.protocol}"
                setOnClickListener {
                    selected = node
                    connectButton.isEnabled = true
                    renderNodes()
                }
            }
            nodeList.addView(row, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                bottomMargin = dp(6)
            })
        }
    }

    private fun onConnectClicked() {
        if (connected) {
            startService(Intent(this, V2SpeedVpnService::class.java).setAction(V2SpeedVpnService.ACTION_STOP))
            return
        }
        val node = selected ?: return
        val permission = VpnService.prepare(this)
        if (permission != null) {
            startActivityForResult(permission, VPN_PERMISSION_REQUEST)
        } else {
            startVpn(node)
        }
    }

    @Deprecated("Android VPN consent result API")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != VPN_PERMISSION_REQUEST) return
        val node = selected
        if (resultCode == RESULT_OK && node != null) startVpn(node)
        else status.text = "مجوز VPN داده نشد؛ تونلی برقرار نشد."
    }

    private fun startVpn(node: ServerNode) {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), NOTIFICATION_REQUEST)
        }
        status.text = "در حال راه‌اندازی تونل برای ${node.name}…"
        val intent = Intent(this, V2SpeedVpnService::class.java)
            .putExtra(V2SpeedVpnService.EXTRA_URI, node.uri)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
    }

    private fun downloadText(address: String): String {
        val connection = URL(address).openConnection() as HttpURLConnection
        connection.connectTimeout = 15000
        connection.readTimeout = 20000
        connection.instanceFollowRedirects = false
        connection.setRequestProperty("User-Agent", "V2Speed-Android-Prototype")
        connection.setRequestProperty("Accept", "text/plain, application/octet-stream")
        try {
            if (connection.responseCode !in 200..299) error("Subscription fetch failed")
            connection.inputStream.use { input ->
                val out = ByteArrayOutputStream()
                val buffer = ByteArray(8192)
                var total = 0
                while (true) {
                    val read = input.read(buffer)
                    if (read < 0) break
                    total += read
                    require(total <= MAX_FEED_BYTES) { "Subscription exceeds size limit" }
                    out.write(buffer, 0, read)
                }
                return out.toString(StandardCharsets.UTF_8.name())
            }
        } finally {
            connection.disconnect()
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    override fun onDestroy() {
        io.shutdownNow()
        super.onDestroy()
    }

    private data class Source(val id: String, val name: String, val url: String)

    companion object {
        private const val VPN_PERMISSION_REQUEST = 801
        private const val NOTIFICATION_REQUEST = 802
        private const val MAX_FEED_BYTES = 1_000_000
        private const val MAX_NODES_PER_SOURCE = 250
        private val SOURCES = listOf(
            Source("iboxz", "Free V2ray Collector · Mix", "https://raw.githubusercontent.com/iboxz/free-v2ray-collector/main/main/mix"),
            Source("thomas", "ThomasJasperthecat · Sublist 1", "https://raw.githubusercontent.com/ThomasJasperthecat/sub/main/sublist1.txt"),
            Source("v2collector", "V2rayCollector · Mixed Iran", "https://raw.githubusercontent.com/youfoundamin/V2rayCollector/refs/heads/main/mixed_iran.txt")
        )
    }
}
