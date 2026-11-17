package app.v2speed.android

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
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import app.v2speed.android.data.EndpointMeasurement
import app.v2speed.android.data.EndpointReachabilityProbe
import app.v2speed.android.data.ServerNode
import app.v2speed.android.data.SmartEndpointRanking
import app.v2speed.android.data.SubscriptionParser
import app.v2speed.android.vpn.SmartConnectSession
import app.v2speed.android.vpn.V2SpeedVpnService
import java.io.ByteArrayOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.concurrent.Callable
import java.util.concurrent.ExecutorCompletionService
import java.util.concurrent.ExecutionException
import java.util.concurrent.Executors
import java.util.concurrent.Future

class MainActivity : Activity() {
    private val io = Executors.newSingleThreadExecutor()
    private val nodes = mutableListOf<ServerNode>()
    private var selected: ServerNode? = null
    private lateinit var page: LinearLayout
    private lateinit var pageScroller: ScrollView
    private lateinit var statusLabel: TextView
    private lateinit var connectionDetail: TextView
    private lateinit var powerButton: TextView
    private lateinit var serverSummary: TextView
    private lateinit var serverList: LinearLayout
    private lateinit var bottomNav: LinearLayout
    private var vpnState = V2SpeedVpnService.STATE_DISCONNECTED
    private var statusMessage = ""
    private var smartSelectionRunning = false
    private var smartSelectionGeneration = 0
    private var smartSelectionTask: Future<*>? = null
    private var pendingSmartBatch: List<EndpointMeasurement>? = null
    private val serviceLatencies = mutableMapOf<String, Triple<Long, Long, Long>>()
    private var visibleNodeCount = INITIAL_VISIBLE_NODES
    private var receiverRegistered = false
    private var lastStateEventId = 0L
    private var activeTab = TAB_HOME

    private val stateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val eventId = intent?.getLongExtra(V2SpeedVpnService.EXTRA_EVENT_ID, 0L) ?: return
            if (eventId <= lastStateEventId) return
            lastStateEventId = eventId
            vpnState = intent.getStringExtra(V2SpeedVpnService.EXTRA_STATE) ?: return
            val message = intent.getStringExtra(V2SpeedVpnService.EXTRA_MESSAGE).orEmpty()
            statusMessage = message
            if (intent.getBooleanExtra(V2SpeedVpnService.EXTRA_PROBE_COMPLETE, false)) {
                val rankedIds = intent.getStringArrayListExtra(V2SpeedVpnService.EXTRA_RANKED_NODE_IDS).orEmpty()
                val instagram = intent.getLongArrayExtra(V2SpeedVpnService.EXTRA_INSTAGRAM_RTT) ?: longArrayOf()
                val youtube = intent.getLongArrayExtra(V2SpeedVpnService.EXTRA_YOUTUBE_RTT) ?: longArrayOf()
                val endpoints = intent.getLongArrayExtra(V2SpeedVpnService.EXTRA_ENDPOINT_RTT) ?: longArrayOf()
                val originalNodes = nodes.associateBy { it.id }
                nodes.clear()
                serviceLatencies.clear()
                rankedIds.forEachIndexed { index, id ->
                    originalNodes[id]?.let { node ->
                        nodes += node
                        if (index < instagram.size && index < youtube.size && index < endpoints.size) {
                            serviceLatencies[id] = Triple(instagram[index], youtube[index], endpoints[index])
                        }
                    }
                }
                visibleNodeCount = INITIAL_VISIBLE_NODES
            }
            val selectedId = intent.getStringExtra(V2SpeedVpnService.EXTRA_NODE_ID)
            if (intent.getBooleanExtra(V2SpeedVpnService.EXTRA_PROBE_COMPLETE, false)) {
                selected = selectedId?.let { id -> nodes.firstOrNull { it.id == id } }
            } else {
                selectedId?.let { id -> selected = nodes.firstOrNull { it.id == id } }
            }
            renderNodes()
            if (::serverSummary.isInitialized) {
                serverSummary.text = selected?.let {
                    "${it.name}\n${it.endpoint} · ${it.protocol.uppercase()}\nمنبع عمومی: ${it.sourceName}"
                } ?: "هنوز سروری رتبه‌بندی نشده\nبرای مشاهدهٔ نتیجه، اتصال هوشمند را اجرا کنید"
            }
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
        restoreLatestVpnStatus()
    }

    private fun restoreLatestVpnStatus() {
        val snapshot = SmartConnectSession.latestStatus() ?: return
        if (snapshot.eventId <= lastStateEventId) return
        val intent = Intent()
            .putExtra(V2SpeedVpnService.EXTRA_EVENT_ID, snapshot.eventId)
            .putExtra(V2SpeedVpnService.EXTRA_STATE, snapshot.state)
            .putExtra(V2SpeedVpnService.EXTRA_MESSAGE, snapshot.message)
            .putExtra(V2SpeedVpnService.EXTRA_NODE_ID, snapshot.nodeId)
        snapshot.rankedResults?.let { ranked ->
            intent.putExtra(V2SpeedVpnService.EXTRA_PROBE_COMPLETE, true)
            intent.putStringArrayListExtra(
                V2SpeedVpnService.EXTRA_RANKED_NODE_IDS,
                ArrayList(ranked.map { it.nodeId })
            )
            intent.putExtra(V2SpeedVpnService.EXTRA_INSTAGRAM_RTT, ranked.map { it.instagramMs }.toLongArray())
            intent.putExtra(V2SpeedVpnService.EXTRA_YOUTUBE_RTT, ranked.map { it.youtubeMs }.toLongArray())
            intent.putExtra(V2SpeedVpnService.EXTRA_ENDPOINT_RTT, ranked.map { it.endpointMs }.toLongArray())
        }
        stateReceiver.onReceive(this, intent)
    }

    override fun onStop() {
        if (receiverRegistered) unregisterReceiver(stateReceiver)
        receiverRegistered = false
        super.onStop()
    }

    private fun buildShell() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = GradientDrawable(
                GradientDrawable.Orientation.TL_BR,
                intArrayOf(0xff03090c.toInt(), 0xff071418.toInt(), 0xff05090d.toInt())
            )
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
        brand.addView(label("V2Speed NEXT", 19f, C.WHITE, true), matchWrap())
        brand.addView(label("جاویدنامان وطن", 13f, C.GOLD), matchWrap())
        val sunMark = roundLabel("☼", 48, C.CARD, C.GOLD, 25f).apply {
            contentDescription = "تنظیمات V2Speed"
            setOnClickListener { showTab(TAB_SETTINGS) }
        }
        header.addView(menu, LinearLayout.LayoutParams(dp(48), dp(48)))
        header.addView(brand, LinearLayout.LayoutParams(0, dp(52), 1f))
        header.addView(sunMark, LinearLayout.LayoutParams(dp(48), dp(48)))

        pageScroller = ScrollView(this).apply { clipToPadding = false }
        page = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(8), dp(18), dp(14))
        }
        pageScroller.addView(page)
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
        root.addView(pageScroller, LinearLayout.LayoutParams(-1, 0, 1f))
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
        pageScroller.post { pageScroller.scrollTo(0, 0) }
    }

    private fun renderHomePage() {
        page.addView(crest(), LinearLayout.LayoutParams(-1, dp(220)).apply { bottomMargin = dp(12) })

        val connectionCard = card()
        connectionCard.setPadding(dp(16), dp(16), dp(16), dp(16))
        val statusTitle = label("وضعیت اتصال   ◈", 14f, C.MUTED, true)
        statusLabel = label("آمادهٔ اتصال", 23f, C.GREEN, true).apply { gravity = Gravity.CENTER }
        connectionDetail = label("برای آزمون همهٔ سرورها و اتصال به بهترین گزینه، دکمهٔ قدرت را لمس کنید.", 13f, C.MUTED).apply { gravity = Gravity.CENTER }
        connectionCard.addView(statusTitle)
        connectionCard.addView(statusLabel, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(4) })
        connectionCard.addView(connectionDetail, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(2) })

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
        serverSummary = label(selected?.let {
            "${it.name}\n${it.endpoint} · ${it.protocol.uppercase()}\nمنبع عمومی: ${it.sourceName}"
        } ?: "هنوز سروری رتبه‌بندی نشده\nبرای مشاهدهٔ فهرست، اینجا را لمس کنید", 13f, C.WHITE, true)
        serverCard.addView(serverSummary, LinearLayout.LayoutParams(0, -2, 1f))
        serverCard.addView(label("◎", 24f, C.GOLD, true))
        serverCard.addView(label("  ›  مشاهدهٔ سرورها", 13f, C.GREEN, true))
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
        page.addView(label("اتصال هوشمند همهٔ کانفیگ‌ها را می‌سنجد؛ منابع فعلی سه فهرست عمومی‌اند، نه سرور رسمی V2Speed. لمس ردیف فقط جزئیات را نشان می‌دهد.", 13f, C.MUTED), bottomSpace(12))
        val actions = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(actionButton("آزمون همه و اتصال بهترین سرور") { beginSmartConnect() }, bottomSpace(8))
            addView(actionButton("تازه‌سازی سه فهرست عمومی") { loadFeeds() }, LinearLayout.LayoutParams(-1, dp(46)))
        }
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
        page.addView(infoRow("منابع سرور", "سه فهرست اشتراک ثابت در برنامه"), bottomSpace(8))
        val voiceCard = card().apply { setPadding(dp(14), dp(12), dp(14), dp(12)) }
        val voiceSwitch = android.widget.Switch(this).apply {
            text = "اعلان صوتی پس از اتصال"
            textSize = 15f
            setTextColor(C.WHITE)
            isChecked = getSharedPreferences(PREFERENCES, MODE_PRIVATE)
                .getBoolean(KEY_VOICE_ANNOUNCEMENT, true)
            setOnCheckedChangeListener { _, enabled ->
                getSharedPreferences(PREFERENCES, MODE_PRIVATE).edit()
                    .putBoolean(KEY_VOICE_ANNOUNCEMENT, enabled).apply()
            }
        }
        voiceCard.addView(voiceSwitch)
        voiceCard.addView(label("جملهٔ اعلام اتصال با صدای فارسیِ نصب‌شده روی گوشی پخش می‌شود؛ اگر موتور دستگاه صدای فارسی نداشته باشد، چیزی پخش نمی‌شود.", 12f, C.MUTED))
        page.addView(voiceCard, bottomSpace(12))
        page.addView(actionButton("بارگیری دوبارهٔ سرورها") { loadFeeds() }, bottomSpace())
        page.addView(actionButton("بازگشت به خانه") { showTab(TAB_HOME) }, bottomSpace())
        page.addView(label("کیفیت و حریم خصوصی سرورهای شخص ثالث به ارائه‌دهندهٔ آن‌ها وابسته است. اطلاعات حساس اتصال را با دیگران به اشتراک نگذارید.", 13f, C.MUTED))
    }

    private fun crest(): View {
        val frame = FrameLayout(this).apply {
            background = GradientDrawable(
                GradientDrawable.Orientation.TL_BR,
                intArrayOf(0xff0c1918.toInt(), 0xff071014.toInt(), C.BG)
            ).apply { cornerRadius = dp(22).toFloat() }
        }
        val orbit = View(this).apply {
            background = rounded(Color.TRANSPARENT, C.GOLD_DARK, 100, dp(1))
            alpha = 0.75f
        }
        frame.addView(orbit, FrameLayout.LayoutParams(dp(194), dp(194), Gravity.CENTER))
        val halo = TextView(this).apply {
            text = "✦       ☼       ✦"
            gravity = Gravity.CENTER
            textSize = 20f
            setTextColor(C.GOLD)
            alpha = 0.8f
        }
        frame.addView(halo, FrameLayout.LayoutParams(-1, dp(32), Gravity.TOP))
        val emblem = ImageView(this).apply {
            setImageResource(R.drawable.v2speed_lion_sun)
            scaleType = ImageView.ScaleType.FIT_CENTER
            contentDescription = "نشان شیر و خورشید V2Speed"
        }
        frame.addView(emblem, FrameLayout.LayoutParams(dp(176), dp(176), Gravity.CENTER))
        frame.addView(
            label("جاویدنامان وطن", 12f, C.GOLD, true).apply { gravity = Gravity.CENTER },
            FrameLayout.LayoutParams(-1, dp(24), Gravity.BOTTOM)
        )
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
        val previousScrollY = if (::pageScroller.isInitialized) pageScroller.scrollY else 0
        serverList.removeAllViews()
        if (nodes.isEmpty()) {
            serverList.addView(label("هنوز سروری رتبه‌بندی نشده؛ آزمون هوشمند را اجرا کنید.", 14f, C.MUTED).apply {
                setPadding(dp(12), dp(18), dp(12), dp(18))
            })
            return
        }
        nodes.take(visibleNodeCount).forEach { node ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                background = rounded(if (selected?.id == node.id) 0xff17271c.toInt() else C.CARD, if (selected?.id == node.id) C.GREEN else C.BORDER, 16, dp(1))
                setPadding(dp(13), dp(11), dp(13), dp(11))
                setOnClickListener {
                    selected = node
                    if (::serverSummary.isInitialized) {
                        serverSummary.text = "${node.name}\n${node.endpoint} · ${node.protocol.uppercase()}\nمنبع عمومی: ${node.sourceName}"
                    }
                    renderNodes()
                }
            }
            val info = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            info.addView(label(node.name, 14f, C.WHITE, true))
            info.addView(label("${node.endpoint}  ·  ${node.protocol.uppercase()}  ·  ${node.sourceName}", 11f, C.MUTED))
            serviceLatencies[node.id]?.let { (instagramMs, youtubeMs, endpointMs) ->
                info.addView(label("Instagram ${instagramMs} ms  ·  YouTube ${youtubeMs} ms  ·  TCP ${endpointMs} ms", 11f, C.GREEN))
            }
            row.addView(info, LinearLayout.LayoutParams(0, -2, 1f))
            row.addView(label(if (selected?.id == node.id) "✓ جزئیات" else "جزئیات", 12f, C.GREEN, true))
            serverList.addView(row, bottomSpace(7))
        }
        if (visibleNodeCount < nodes.size) {
            serverList.addView(actionButton("نمایش ${minOf(INITIAL_VISIBLE_NODES, nodes.size - visibleNodeCount)} سرور بعدی از ${nodes.size}") {
                visibleNodeCount = (visibleNodeCount + INITIAL_VISIBLE_NODES).coerceAtMost(nodes.size)
                renderNodes()
            }, bottomSpace(12))
        }
        if (::pageScroller.isInitialized) {
            pageScroller.post { pageScroller.scrollTo(0, previousScrollY.coerceAtMost(pageScroller.getChildAt(0)?.height ?: 0)) }
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
            V2SpeedVpnService.STATE_CONNECTED -> "هستهٔ VPN فعال است"
            V2SpeedVpnService.STATE_TESTING -> "در حال آزمون همهٔ کانفیگ‌ها…"
            V2SpeedVpnService.STATE_CONNECTING -> "آزمون‌ها کامل شد؛ در حال اتصال…"
            V2SpeedVpnService.STATE_ERROR -> "اتصال برقرار نشد"
            else -> "آمادهٔ اتصال"
        }
        statusLabel.setTextColor(if (vpnState == V2SpeedVpnService.STATE_ERROR) C.RED else C.GREEN)
        if (::connectionDetail.isInitialized) {
            connectionDetail.text = when (vpnState) {
                V2SpeedVpnService.STATE_CONNECTED -> statusMessage.ifBlank { "تونل شروع شده؛ دسترسی اینترنت مستقل بررسی نشده است." }
                V2SpeedVpnService.STATE_TESTING -> statusMessage.ifBlank { "آزمون تمام کانفیگ‌های پاسخ‌گو در حال اجراست…" }
                V2SpeedVpnService.STATE_CONNECTING -> statusMessage.ifBlank { "آزمون‌ها کامل شد؛ در حال برقراری تونل…" }
                V2SpeedVpnService.STATE_ERROR -> statusMessage.ifBlank { "پیکربندی یا دسترسی شبکه را بررسی کنید." }
                else -> selected?.let { "جزئیات ${it.name} · ${it.sourceName}" } ?: "برای آزمون همه و اتصال بهترین سرور، دکمهٔ قدرت را لمس کنید."
            }
        }
        powerButton.text = "⏻"
        powerButton.setTextColor(if (connected) C.GREEN else C.GOLD)
        powerButton.setBackgroundDrawable(rounded(C.CARD, if (connected) C.GREEN else C.GOLD_DARK, 100, dp(2)))
    }

    private fun isConnected(): Boolean = vpnState == V2SpeedVpnService.STATE_CONNECTED ||
        vpnState == V2SpeedVpnService.STATE_CONNECTING || vpnState == V2SpeedVpnService.STATE_TESTING

    private fun onPowerClicked() {
        if (isConnected() || smartSelectionRunning) {
            if (smartSelectionRunning) {
                smartSelectionRunning = false
                smartSelectionGeneration++
                smartSelectionTask?.cancel(true)
                smartSelectionTask = null
                nodes.clear()
                serviceLatencies.clear()
                selected = null
                visibleNodeCount = INITIAL_VISIBLE_NODES
                renderNodes()
                if (::serverSummary.isInitialized) serverSummary.text = "آزمون لغو شد؛ سروری تأیید نشده است."
                powerButton.isEnabled = true
            }
            startService(Intent(this, V2SpeedVpnService::class.java).setAction(V2SpeedVpnService.ACTION_STOP))
            vpnState = V2SpeedVpnService.STATE_DISCONNECTED
            pendingSmartBatch = null
            updateConnectionUi()
            statusLabel.text = "در حال توقف تونل…"
            return
        }
        beginSmartConnect()
    }

    private fun beginSmartConnect() {
        if (isConnected() || smartSelectionRunning || !::powerButton.isInitialized) return
        smartSelectionRunning = true
        val generation = ++smartSelectionGeneration
        powerButton.text = "×"
        statusLabel.text = "در حال دریافت و اعتبارسنجی کانفیگ‌ها…"
        connectionDetail.text = "تمام کانفیگ‌های قابل‌خواندن از سه فهرست عمومی بررسی می‌شوند؛ پیش‌آزمون TCP برای هر مورد اجرا می‌شود."
        statusLabel.setTextColor(C.GREEN)
        smartSelectionTask = io.submit {
            val fetched = mutableListOf<ServerNode>()
            val unavailableSources = mutableListOf<String>()
            for ((sourceIndex, source) in SOURCES.withIndex()) {
                if (Thread.currentThread().isInterrupted) return@submit
                runOnUiThread {
                    if (generation == smartSelectionGeneration && smartSelectionRunning) {
                        statusLabel.text = "در حال دریافت فهرست ${sourceIndex + 1} از ${SOURCES.size}…"
                        connectionDetail.text = source.name
                    }
                }
                try {
                    val body = downloadText(source.url)
                    fetched += SubscriptionParser.parseFeed(source.id, source.name, body)
                } catch (_: Exception) {
                    unavailableSources += source.name
                }
            }
            if (Thread.currentThread().isInterrupted) return@submit
            val configValid = fetched.filter { node ->
                runCatching { SubscriptionParser.toXrayConfig(node.uri) }.isSuccess
            }
            val scanCandidates = configValid.distinctBy { it.uri.substringBefore('#') }
            runOnUiThread {
                if (generation == smartSelectionGeneration && smartSelectionRunning) {
                    statusLabel.text = "در حال پیش‌آزمون TCP همهٔ کانفیگ‌ها…"
                    connectionDetail.text = "${scanCandidates.size} کانفیگ قابل‌ساخت؛ بررسی موازی با حداکثر ۶ اتصال هم‌زمان"
                }
            }
            val measurements = probeEndpoints(scanCandidates, generation)
            val ranked = SmartEndpointRanking.rank(measurements, measurements.size)
            if (Thread.currentThread().isInterrupted) return@submit
            runOnUiThread {
                if (generation != smartSelectionGeneration) return@runOnUiThread
                smartSelectionTask = null
                smartSelectionRunning = false
                powerButton.isEnabled = true
                nodes.clear()
                nodes += ranked.map { it.node }
                serviceLatencies.clear()
                visibleNodeCount = INITIAL_VISIBLE_NODES
                selected = null
                renderNodes()
                if (::serverSummary.isInitialized) {
                    serverSummary.text = if (ranked.isEmpty()) {
                        "کانفیگ قابل‌آزمونی پیدا نشد\nفهرست‌های عمومی را تازه‌سازی کنید"
                    } else {
                        "${ranked.size} کانفیگ از ${fetched.size} موردِ قابل‌ساخت، در آزمون TCP پاسخ داد\nهمهٔ این موارد پیش از اتصال، جداگانه برای Instagram و YouTube آزموده می‌شوند"
                    }
                }
                if (ranked.isEmpty()) {
                    statusLabel.text = "کانفیگ قابل‌دسترسی پیدا نشد"
                    connectionDetail.text = "${fetched.size - configValid.size} پیکربندی قابل‌ساخت نبود؛ از ${scanCandidates.size} کانفیگ، ${scanCandidates.size - measurements.size} سرور در آزمون TCP پاسخ نداد."
                    statusLabel.setTextColor(C.RED)
                    toast("اتصال انجام نشد؛ کانفیگ نامعتبر یا سرورهای در دسترس وجود ندارد.")
                    return@runOnUiThread
                }
                val unavailableText = if (unavailableSources.isEmpty()) "" else " · بعضی فهرست‌ها در دسترس نبودند"
                statusLabel.text = "در حال آزمون همهٔ سرورهای پاسخ‌گو…"
                connectionDetail.text = "${ranked.size} کانفیگ از ${scanCandidates.size} کانفیگ اشتراک، در حال آزمون HTTPS هر دو سرویس است؛ تا پایان بررسی همه، اتصالی آغاز نمی‌شود$unavailableText"
                renderNodes()
                val permission = VpnService.prepare(this)
                if (permission != null) {
                    pendingSmartBatch = ranked
                    startActivityForResult(permission, VPN_PERMISSION_REQUEST)
                } else {
                    startSmartVpn(ranked)
                }
            }
        }
    }

    private fun probeEndpoints(candidates: List<ServerNode>, generation: Int): List<EndpointMeasurement> {
        if (candidates.isEmpty()) return emptyList()
        val executor = Executors.newFixedThreadPool(6)
        val completions = ExecutorCompletionService<EndpointMeasurement?>(executor)
        return try {
            candidates.forEach { node ->
                completions.submit(Callable { EndpointReachabilityProbe.measure(node) })
            }
            val measurements = ArrayList<EndpointMeasurement>()
            val progressInterval = (candidates.size / 100).coerceAtLeast(1)
            for (completed in 1..candidates.size) {
                if (Thread.currentThread().isInterrupted) throw InterruptedException()
                val measurement = try {
                    completions.take().get()
                } catch (_: ExecutionException) {
                    null
                }
                measurement?.let(measurements::add)
                if (completed % progressInterval == 0 || completed == candidates.size) {
                    val reachable = measurements.size
                    runOnUiThread {
                        if (generation == smartSelectionGeneration && smartSelectionRunning) {
                            statusLabel.text = "پیش‌آزمون TCP: $completed از ${candidates.size}"
                            connectionDetail.text = "$reachable سرور پاسخ داد؛ بقیه پس از پایان سنجش کنار گذاشته می‌شوند."
                        }
                    }
                }
            }
            measurements
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            emptyList()
        } finally {
            executor.shutdownNow()
        }
    }

    private fun startSmartVpn(batch: List<EndpointMeasurement>) {
        if (batch.isEmpty()) return
        pendingSmartBatch = null
        vpnState = V2SpeedVpnService.STATE_TESTING
        statusMessage = "در حال آزمون واقعی HTTPS از مسیر Xray برای همهٔ کانفیگ‌ها…"
        updateConnectionUi()
        statusLabel.text = "در حال آزمون همهٔ کانفیگ‌ها…"
        connectionDetail.text = "پروب‌ها فقط وضعیت دسترسی و زمان پاسخ HTTPS را می‌سنجند؛ سرعت و پایداری استریم را تضمین نمی‌کنند."
        val sessionId = SmartConnectSession.stage(batch.map {
            SmartConnectSession.Candidate(it.node.id, it.node.uri, it.latencyMs)
        })
        val intent = Intent(this, V2SpeedVpnService::class.java)
            .putExtra(V2SpeedVpnService.EXTRA_SMART_SESSION_ID, sessionId)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
    }

    private fun loadFeeds() {
        if (smartSelectionRunning || isConnected()) {
            toast("هنگام آزمون یا اتصال، فهرست را نمی‌توان جایگزین کرد.")
            return
        }
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
                    loaded += SubscriptionParser.parseFeed(source.id, source.name, response)
                } catch (_: Exception) {
                    failures += source.name
                }
            }
            runOnUiThread {
                nodes.clear()
                nodes += loaded
                serviceLatencies.clear()
                visibleNodeCount = INITIAL_VISIBLE_NODES
                if (selected != null && nodes.none { it.id == selected?.id }) selected = null
                renderNodes()
                if (::serverSummary.isInitialized) {
                    serverSummary.text = selected?.let {
                        "${it.name}\n${it.endpoint} · ${it.protocol.uppercase()}\nمنبع عمومی: ${it.sourceName}"
                    } ?: "هنوز سروری رتبه‌بندی نشده\nبرای مشاهدهٔ نتیجه، اتصال هوشمند را اجرا کنید"
                }
                if (::statusLabel.isInitialized && activeTab == TAB_HOME && vpnState == V2SpeedVpnService.STATE_DISCONNECTED) {
                    statusLabel.text = when {
                        loaded.isEmpty() -> "هیچ سروری بارگیری نشد"
                        failures.isNotEmpty() -> "${loaded.size} سرور بارگیری شد؛ بعضی منابع در دسترس نبودند"
                        else -> "${loaded.size} کانفیگ بارگیری شد؛ برای اتصال هوشمند لمس کنید"
                    }
                }
            }
        }
    }

    @Deprecated("Android VPN consent result API")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != VPN_PERMISSION_REQUEST) return
        val smartBatch = pendingSmartBatch
        if (resultCode == RESULT_OK && smartBatch != null) {
            startSmartVpn(smartBatch)
        } else {
            pendingSmartBatch = null
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
        background = GradientDrawable(
            GradientDrawable.Orientation.TL_BR,
            intArrayOf(0xff122024.toInt(), 0xff0a1317.toInt())
        ).apply {
            cornerRadius = dp(20).toFloat()
            setStroke(dp(1), C.BORDER)
        }
    }

    private fun infoRow(title: String, value: String): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        background = GradientDrawable(
            GradientDrawable.Orientation.TL_BR,
            intArrayOf(0xff101d20.toInt(), C.CARD)
        ).apply {
            cornerRadius = dp(14).toFloat()
            setStroke(dp(1), C.BORDER)
        }
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
        const val BG = 0xff05090d.toInt()
        const val CARD = 0xff0d1519.toInt()
        const val BORDER = 0xff30423d.toInt()
        const val WHITE = 0xfff4f6f3.toInt()
        const val MUTED = 0xffa2b0ac.toInt()
        const val GOLD = 0xffd6a443.toInt()
        const val GOLD_DARK = 0xff80632f.toInt()
        const val GREEN = 0xff62df88.toInt()
        const val RED = 0xffff7777.toInt()
    }

    private data class Source(val id: String, val name: String, val url: String)

    companion object {
        const val PREFERENCES = "v2speed_preferences"
        const val KEY_VOICE_ANNOUNCEMENT = "voice_announcement_after_connect"
        private const val TAB_HOME = 0
        private const val TAB_SERVERS = 1
        private const val TAB_SETTINGS = 2
        private const val VPN_PERMISSION_REQUEST = 801
        private const val MAX_FEED_BYTES = 1_000_000
        private const val INITIAL_VISIBLE_NODES = 50
        private val SOURCES = listOf(
            Source("iboxz", "Free V2ray Collector · Mix", "https://raw.githubusercontent.com/iboxz/free-v2ray-collector/main/main/mix"),
            Source("thomas", "ThomasJasperthecat · Sublist 1", "https://raw.githubusercontent.com/ThomasJasperthecat/sub/main/sublist1.txt"),
            Source("v2collector", "V2rayCollector · Mixed Iran", "https://raw.githubusercontent.com/youfoundamin/V2rayCollector/refs/heads/main/mixed_iran.txt")
        )
    }
}
