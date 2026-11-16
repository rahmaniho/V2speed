package app.v2speed.android

import android.Manifest
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.VpnService
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
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
    private lateinit var page: LinearLayout
    private lateinit var statusLabel: TextView
    private lateinit var powerButton: TextView
    private lateinit var serverSummary: TextView
    private lateinit var serverList: LinearLayout
    private lateinit var bottomNav: LinearLayout
    private var vpnState = V2SpeedVpnService.STATE_DISCONNECTED
    private var receiverRegistered = false
    private var activeTab = TAB_HOME

    private val stateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            vpnState = intent?.getStringExtra(V2SpeedVpnService.EXTRA_STATE) ?: return
            val message = intent.getStringExtra(V2SpeedVpnService.EXTRA_MESSAGE).orEmpty()
            statusLabel.text = message
            updateConnectionUi()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildShell()
        showTab(TAB_HOME)
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

    private fun buildShell() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(C.BG)
            layoutDirection = View.LAYOUT_DIRECTION_RTL
        }
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(18), dp(14), dp(18), dp(8))
        }
        val menu = roundLabel("☰", 48, C.CARD, C.GOLD, 22f).apply {
            setOnClickListener { showTab(TAB_SETTINGS) }
        }
        val brand = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
        }
        brand.addView(label("به V2Speed خوش آمدید", 19f, C.WHITE, true), matchWrap())
        brand.addView(label("امنیت، سرعت، آزادی", 13f, C.MUTED), matchWrap())
        val crown = roundLabel("♛", 48, C.CARD, C.GOLD, 25f).apply {
            setOnClickListener { toast("بخش ویژه هنوز فعال نشده است.") }
        }
        header.addView(menu, LinearLayout.LayoutParams(dp(48), dp(48)))
        header.addView(brand, LinearLayout.LayoutParams(0, dp(52), 1f))
        header.addView(crown, LinearLayout.LayoutParams(dp(48), dp(48)))

        val scroller = ScrollView(this).apply { clipToPadding = false }
        page = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(8), dp(18), dp(14))
        }
        scroller.addView(page)
        bottomNav = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            background = rounded(C.CARD, C.BORDER, 24)
            setPadding(dp(8), dp(5), dp(8), dp(5))
        }
        val navWrap = LinearLayout(this).apply {
            setPadding(dp(16), dp(4), dp(16), dp(12))
            addView(bottomNav, LinearLayout.LayoutParams(-1, dp(66)))
        }
        root.addView(header)
        root.addView(scroller, LinearLayout.LayoutParams(-1, 0, 1f))
        root.addView(navWrap)
        setContentView(root)
        renderBottomNav()
    }

    private fun showTab(tab: Int) {
        activeTab = tab
        page.removeAllViews()
        when (tab) {
            TAB_SERVERS -> renderServersPage()
            TAB_SETTINGS -> renderSettingsPage()
            else -> renderHomePage()
        }
        renderBottomNav()
    }

    private fun renderHomePage() {
        page.addView(crest(), LinearLayout.LayoutParams(-1, dp(190)).apply { bottomMargin = dp(12) })

        val connectionCard = card()
        connectionCard.setPadding(dp(16), dp(16), dp(16), dp(16))
        val statusTitle = label("وضعیت اتصال   ◈", 14f, C.MUTED, true)
        statusLabel = label("آمادهٔ اتصال", 23f, C.GREEN, true).apply { gravity = Gravity.CENTER }
        val detail = label("برای شروع، سروری انتخاب کنید", 13f, C.MUTED).apply { gravity = Gravity.CENTER }
        connectionCard.addView(statusTitle)
        connectionCard.addView(statusLabel, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(4) })
        connectionCard.addView(detail, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(2) })

        val controlRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            setPadding(0, dp(10), 0, dp(6))
        }
        controlRow.addView(metricCard("↓", "دانلود", "—", 1f))
        val powerShell = FrameLayout(this).apply {
            background = rounded(C.BG, if (isConnected()) C.GREEN else C.BORDER, 3, dp(3))
            setPadding(dp(7), dp(7), dp(7), dp(7))
        }
        powerButton = roundLabel("⏻", 92, C.CARD, if (isConnected()) C.GREEN else C.GOLD, 43f).apply {
            setOnClickListener { onPowerClicked() }
            contentDescription = "شروع یا توقف اتصال VPN"
        }
        powerShell.addView(powerButton, FrameLayout.LayoutParams(dp(92), dp(92), Gravity.CENTER))
        controlRow.addView(powerShell, LinearLayout.LayoutParams(dp(112), dp(112)).apply {
            leftMargin = dp(8); rightMargin = dp(8)
        })
        controlRow.addView(metricCard("↑", "آپلود", "—", 1f))
        connectionCard.addView(controlRow)
        connectionCard.addView(label("سرعت‌سنج در این نسخه اندازه‌گیری نمی‌شود", 12f, C.MUTED).apply { gravity = Gravity.CENTER })
        page.addView(connectionCard, bottomSpace())

        val serverCard = card().apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            setOnClickListener { showTab(TAB_SERVERS) }
        }
        serverSummary = label(selected?.let { "${it.name}\n${it.endpoint} · ${it.protocol.uppercase()}" } ?: "سروری انتخاب نشده\nبرای انتخاب، اینجا را لمس کنید", 14f, C.WHITE, true)
        serverCard.addView(serverSummary, LinearLayout.LayoutParams(0, -2, 1f))
        serverCard.addView(label("◎", 24f, C.GOLD, true))
        serverCard.addView(label("  ›  تغییر سرور", 13f, C.GREEN, true))
        page.addView(serverCard, bottomSpace(12))

        val features = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        features.addView(featureTile("◉", "سرعت‌سنج", "اندازه‌گیری نشده"), LinearLayout.LayoutParams(0, dp(102), 1f).apply { rightMargin = dp(4) })
        features.addView(featureTile("◇", "حفاظت", "VPN اندروید"), LinearLayout.LayoutParams(0, dp(102), 1f).apply { leftMargin = dp(4); rightMargin = dp(4) })
        features.addView(featureTile("◎", "DNS", "بررسی نشده"), LinearLayout.LayoutParams(0, dp(102), 1f).apply { leftMargin = dp(4); rightMargin = dp(4) })
        features.addView(featureTile("⏻", "Kill Switch", "غیرفعال"), LinearLayout.LayoutParams(0, dp(102), 1f).apply { leftMargin = dp(4) })
        page.addView(features, bottomSpace(12))

        val note = card().apply { setPadding(dp(14), dp(13), dp(14), dp(13)) }
        note.addView(label("نسخهٔ آزمایشی", 16f, C.GREEN, true))
        note.addView(label("اتصال و سرعت سرورها تضمین نمی‌شود. سنجش سرعت، نشت DNS و Kill Switch هنوز پیاده‌سازی نشده‌اند.", 12f, C.MUTED).apply {
            setPadding(0, dp(5), 0, 0)
        })
        page.addView(note, bottomSpace())
        updateConnectionUi()
    }

    private fun renderServersPage() {
        page.addView(label("سرورها", 23f, C.WHITE, true), bottomSpace(6))
        page.addView(label("فقط سه منبع اشتراک تعریف‌شده در برنامه بارگیری می‌شوند.", 13f, C.MUTED), bottomSpace(12))
        val actions = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        actions.addView(actionButton("بارگیری سرورها") { loadFeeds() }, LinearLayout.LayoutParams(0, dp(48), 1f))
        actions.addView(actionButton("خانه") { showTab(TAB_HOME) }, LinearLayout.LayoutParams(0, dp(48), 1f).apply { rightMargin = dp(8) })
        page.addView(actions, bottomSpace())
        serverList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        page.addView(serverList)
        renderNodes()
    }

    private fun renderSettingsPage() {
        page.addView(label("تنظیمات", 23f, C.WHITE, true), bottomSpace(12))
        page.addView(infoRow("حالت تونل", "VPN سیستمی Android"), bottomSpace(8))
        page.addView(infoRow("پشتیبانی", "Android 10 و بالاتر"), bottomSpace(8))
        page.addView(infoRow("سرعت‌سنج", "هنوز پیاده‌سازی نشده"), bottomSpace(8))
        page.addView(infoRow("Kill Switch و آزمون DNS", "هنوز پیاده‌سازی نشده"), bottomSpace(8))
        page.addView(infoRow("منابع سرور", "سه فهرست اشتراک ثابت در برنامه"), bottomSpace(12))
        page.addView(actionButton("بارگیری دوبارهٔ سرورها") { loadFeeds() }, bottomSpace())
        page.addView(actionButton("بازگشت به خانه") { showTab(TAB_HOME) }, bottomSpace())
        page.addView(label("کیفیت و حریم خصوصی سرورهای شخص ثالث به ارائه‌دهندهٔ آن‌ها وابسته است. اطلاعات حساس اتصال را با دیگران به اشتراک نگذارید.", 13f, C.MUTED))
    }

    private fun crest(): View {
        val frame = FrameLayout(this).apply {
            background = GradientDrawable(GradientDrawable.Orientation.TL_BR, intArrayOf(0xff15130f.toInt(), 0xff0b0d12.toInt(), C.BG)).apply { cornerRadius = dp(20).toFloat() }
        }
        val halo = TextView(this).apply {
            text = "✦       ☼       ✦"
            gravity = Gravity.CENTER
            textSize = 22f
            setTextColor(C.GOLD)
            alpha = 0.8f
        }
        frame.addView(halo, FrameLayout.LayoutParams(-1, dp(45), Gravity.TOP))
        val seal = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            background = rounded(0xff10251b.toInt(), C.GOLD_DARK, 100, dp(2))
            setPadding(dp(18), dp(12), dp(18), dp(12))
        }
        seal.addView(label("♛", 66f, C.GOLD, true).apply { gravity = Gravity.CENTER })
        seal.addView(label("V2SPEED", 13f, C.GOLD, true).apply { gravity = Gravity.CENTER; letterSpacing = 0.12f })
        frame.addView(seal, FrameLayout.LayoutParams(dp(148), dp(148), Gravity.CENTER))
        val caption = label("امنیت  ·  سرعت  ·  آزادی", 12f, C.MUTED).apply { gravity = Gravity.CENTER }
        frame.addView(caption, FrameLayout.LayoutParams(-1, dp(25), Gravity.BOTTOM))
        return frame
    }

    private fun metricCard(icon: String, title: String, value: String, weight: Float): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        gravity = Gravity.CENTER
        background = rounded(C.CARD, C.BORDER, 18, dp(1))
        setPadding(dp(4), dp(8), dp(4), dp(8))
        addView(label(icon, 24f, C.GREEN, true).apply { gravity = Gravity.CENTER })
        addView(label(title, 12f, C.MUTED).apply { gravity = Gravity.CENTER })
        addView(label(value, 15f, C.GREEN, true).apply { gravity = Gravity.CENTER })
    }

    private fun featureTile(icon: String, title: String, subtitle: String): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        gravity = Gravity.CENTER
        background = rounded(C.CARD, C.BORDER, 16, dp(1))
        setPadding(dp(2), dp(6), dp(2), dp(6))
        addView(label(icon, 25f, C.GOLD, true).apply { gravity = Gravity.CENTER })
        addView(label(title, 11f, C.WHITE, true).apply { gravity = Gravity.CENTER })
        addView(label(subtitle, 9f, C.MUTED).apply { gravity = Gravity.CENTER })
        setOnClickListener {
            val explanation = when (title) {
                "سرعت‌سنج" -> "آزمون سرعت هنوز پیاده‌سازی نشده است؛ عدد ساختگی نشان نمی‌دهیم."
                "DNS" -> "آزمون نشت DNS هنوز پیاده‌سازی نشده است."
                "Kill Switch" -> "Kill Switch در این نسخه فعال نیست."
                else -> "تونل Android VPN از هستهٔ Xray استفاده می‌کند؛ امنیت سرورهای عمومی تضمین‌شده نیست."
            }
            toast(explanation)
        }
    }

    private fun renderNodes() {
        if (!::serverList.isInitialized) return
        serverList.removeAllViews()
        if (nodes.isEmpty()) {
            serverList.addView(label("در حال دریافت سرورها یا هنوز فهرستی بارگیری نشده است.", 14f, C.MUTED).apply {
                setPadding(dp(12), dp(18), dp(12), dp(18))
            })
            return
        }
        nodes.forEach { node ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                background = rounded(if (selected?.id == node.id) 0xff17271c.toInt() else C.CARD, if (selected?.id == node.id) C.GREEN else C.BORDER, 16, dp(1))
                setPadding(dp(13), dp(11), dp(13), dp(11))
                setOnClickListener {
                    selected = node
                    if (::serverSummary.isInitialized) serverSummary.text = "${node.name}\n${node.endpoint} · ${node.protocol.uppercase()}"
                    renderNodes()
                }
            }
            val info = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            info.addView(label(node.name, 14f, C.WHITE, true))
            info.addView(label("${node.endpoint}  ·  ${node.protocol.uppercase()}  ·  ${node.sourceName}", 11f, C.MUTED))
            row.addView(info, LinearLayout.LayoutParams(0, -2, 1f))
            row.addView(label(if (selected?.id == node.id) "✓ انتخاب‌شده" else "انتخاب", 12f, C.GREEN, true))
            serverList.addView(row, bottomSpace(7))
        }
    }

    private fun renderBottomNav() {
        if (!::bottomNav.isInitialized) return
        bottomNav.removeAllViews()
        listOf(Triple(TAB_SERVERS, "◎", "سرورها"), Triple(TAB_HOME, "⌂", "خانه"), Triple(TAB_SETTINGS, "⚙", "تنظیمات")).forEach { item ->
            val selectedTab = activeTab == item.first
            val tile = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                setOnClickListener { showTab(item.first) }
                addView(label(item.second, 23f, if (selectedTab) C.GOLD else C.MUTED, true).apply { gravity = Gravity.CENTER })
                addView(label(item.third, 11f, if (selectedTab) C.GOLD else C.MUTED).apply { gravity = Gravity.CENTER })
            }
            bottomNav.addView(tile, LinearLayout.LayoutParams(0, -1, 1f))
        }
    }

    private fun updateConnectionUi() {
        if (!::statusLabel.isInitialized) return
        val connected = isConnected()
        statusLabel.text = when (vpnState) {
            V2SpeedVpnService.STATE_CONNECTED -> "متصل هستید"
            V2SpeedVpnService.STATE_CONNECTING -> "در حال اتصال…"
            V2SpeedVpnService.STATE_ERROR -> "اتصال برقرار نشد"
            else -> "آمادهٔ اتصال"
        }
        statusLabel.setTextColor(if (vpnState == V2SpeedVpnService.STATE_ERROR) C.RED else C.GREEN)
        powerButton.text = if (connected) "⏻" else "⏻"
        powerButton.setTextColor(if (connected) C.GREEN else C.GOLD)
        powerButton.setBackgroundDrawable(rounded(C.CARD, if (connected) C.GREEN else C.GOLD_DARK, 100, dp(2)))
    }

    private fun isConnected(): Boolean = vpnState == V2SpeedVpnService.STATE_CONNECTED || vpnState == V2SpeedVpnService.STATE_CONNECTING

    private fun onPowerClicked() {
        if (isConnected()) {
            startService(Intent(this, V2SpeedVpnService::class.java).setAction(V2SpeedVpnService.ACTION_STOP))
            vpnState = V2SpeedVpnService.STATE_DISCONNECTED
            updateConnectionUi()
            statusLabel.text = "در حال توقف تونل…"
            return
        }
        val node = selected
        if (node == null) {
            showTab(TAB_SERVERS)
            toast("ابتدا یک سرور انتخاب کنید.")
            return
        }
        val permission = VpnService.prepare(this)
        if (permission != null) startActivityForResult(permission, VPN_PERMISSION_REQUEST) else startVpn(node)
    }

    private fun loadFeeds() {
        if (::statusLabel.isInitialized && activeTab == TAB_HOME) statusLabel.text = "در حال دریافت فهرست سرورها…"
        if (::serverList.isInitialized) {
            serverList.removeAllViews()
            serverList.addView(label("در حال دریافت فهرست سرورها…", 14f, C.MUTED).apply { setPadding(dp(12), dp(18), dp(12), dp(18)) })
        }
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
                if (selected != null && nodes.none { it.id == selected?.id }) selected = null
                renderNodes()
                if (::serverSummary.isInitialized) serverSummary.text = selected?.let { "${it.name}\n${it.endpoint} · ${it.protocol.uppercase()}" } ?: "سروری انتخاب نشده\nبرای انتخاب، اینجا را لمس کنید"
                if (::statusLabel.isInitialized && activeTab == TAB_HOME && vpnState == V2SpeedVpnService.STATE_DISCONNECTED) {
                    statusLabel.text = when {
                        loaded.isEmpty() -> "هیچ سروری بارگیری نشد"
                        failures.isNotEmpty() -> "${loaded.size} سرور بارگیری شد؛ بعضی منابع در دسترس نبودند"
                        else -> "${loaded.size} سرور بارگیری شد؛ یکی را انتخاب کنید"
                    }
                }
            }
        }
    }

    private fun startVpn(node: ServerNode) {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), NOTIFICATION_REQUEST)
        }
        vpnState = V2SpeedVpnService.STATE_CONNECTING
        updateConnectionUi()
        statusLabel.text = "در حال راه‌اندازی تونل برای ${node.name}…"
        val intent = Intent(this, V2SpeedVpnService::class.java).putExtra(V2SpeedVpnService.EXTRA_URI, node.uri)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
    }

    @Deprecated("Android VPN consent result API")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != VPN_PERMISSION_REQUEST) return
        val node = selected
        if (resultCode == RESULT_OK && node != null) startVpn(node)
        else {
            vpnState = V2SpeedVpnService.STATE_DISCONNECTED
            updateConnectionUi()
            statusLabel.text = "مجوز VPN داده نشد؛ تونلی برقرار نشد."
        }
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

    private fun card(): LinearLayout = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        background = rounded(C.CARD, C.BORDER, 20, dp(1))
    }

    private fun infoRow(title: String, value: String): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        background = rounded(C.CARD, C.BORDER, 14, dp(1))
        setPadding(dp(14), dp(12), dp(14), dp(12))
        addView(label(title, 13f, C.MUTED))
        addView(label(value, 15f, C.WHITE, true).apply { setPadding(0, dp(3), 0, 0) })
    }

    private fun actionButton(text: String, action: () -> Unit): TextView = roundLabel(text, 48, C.CARD, C.GREEN, 14f).apply { setOnClickListener { action() } }

    private fun roundLabel(text: String, size: Int, bg: Int, fg: Int, textSize: Float): TextView = label(text, textSize, fg, true).apply {
        gravity = Gravity.CENTER
        background = rounded(bg, C.BORDER, 18, dp(1))
        minWidth = dp(size)
        minHeight = dp(size)
    }

    private fun label(text: String, size: Float, color: Int, bold: Boolean = false): TextView = TextView(this).apply {
        this.text = text
        textSize = size
        setTextColor(color)
        if (bold) setTypeface(typeface, Typeface.BOLD)
        gravity = Gravity.CENTER_VERTICAL
        includeFontPadding = true
    }

    private fun rounded(color: Int, stroke: Int, radius: Int, strokeWidth: Int = 0): GradientDrawable = GradientDrawable().apply {
        setColor(color)
        cornerRadius = dp(radius).toFloat()
        if (strokeWidth > 0) setStroke(strokeWidth, stroke)
    }

    private fun bottomSpace(bottom: Int = 10): LinearLayout.LayoutParams = LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(bottom) }
    private fun matchWrap() = LinearLayout.LayoutParams(-1, -2)
    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()
    private fun toast(message: String) = Toast.makeText(this, message, Toast.LENGTH_LONG).show()

    override fun onDestroy() {
        io.shutdownNow()
        super.onDestroy()
    }

    private object C {
        const val BG = 0xff090b0f.toInt()
        const val CARD = 0xff15171c.toInt()
        const val BORDER = 0xff383a40.toInt()
        const val WHITE = 0xfff4f2ed.toInt()
        const val MUTED = 0xffa6a5a2.toInt()
        const val GOLD = 0xffe2b955.toInt()
        const val GOLD_DARK = 0xff896b2d.toInt()
        const val GREEN = 0xff51c878.toInt()
        const val RED = 0xffff7777.toInt()
    }

    private data class Source(val id: String, val name: String, val url: String)

    companion object {
        private const val TAB_HOME = 0
        private const val TAB_SERVERS = 1
        private const val TAB_SETTINGS = 2
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
