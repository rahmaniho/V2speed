package app.v2speed.android.vpn

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Intent
import android.net.VpnService
import android.os.Build
import android.os.ParcelFileDescriptor
import android.content.pm.ServiceInfo
import android.media.AudioAttributes
import android.os.Bundle
import android.speech.tts.TextToSpeech
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.Locale
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import app.v2speed.android.MainActivity
import app.v2speed.android.data.SmartProbeCandidate
import app.v2speed.android.data.SmartServiceRanking
import app.v2speed.android.data.SubscriptionParser

class V2SpeedVpnService : VpnService() {
    private var tunnel: ParcelFileDescriptor? = null
    private var core: XrayCoreAdapter? = null
    private var speech: TextToSpeech? = null
    private var speechReady = false
    private var queuedAnnouncement: String? = null
    private val speechLock = Any()
    private val stopRequested = AtomicBoolean(false)
    private var smartProbeInProgress = false
    private val commands = Executors.newSingleThreadExecutor { task -> Thread(task, "v2speed-vpn-lifecycle") }

    override fun onCreate() {
        super.onCreate()
        speech = TextToSpeech(applicationContext) { result ->
            if (result != TextToSpeech.SUCCESS) return@TextToSpeech
            synchronized(speechLock) {
                val engine = speech ?: return@synchronized
                var languageResult = engine.setLanguage(Locale("fa", "IR"))
                if (languageResult == TextToSpeech.LANG_MISSING_DATA || languageResult == TextToSpeech.LANG_NOT_SUPPORTED) {
                    languageResult = engine.setLanguage(Locale("fa"))
                }
                if (languageResult != TextToSpeech.LANG_MISSING_DATA && languageResult != TextToSpeech.LANG_NOT_SUPPORTED) {
                    engine.setSpeechRate(0.84f)
                    engine.setPitch(0.90f)
                    engine.setAudioAttributes(
                        AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY)
                            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                            .build()
                    )
                    speechReady = true
                    speakQueuedAnnouncementLocked()
                }
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopRequested.set(true)
            commands.execute { stopTunnel() }
            return START_NOT_STICKY
        }
        stopRequested.set(false)
        val smartSessionId = intent?.getStringExtra(EXTRA_SMART_SESSION_ID)
        if (!smartSessionId.isNullOrBlank()) {
            val candidates = SmartConnectSession.take(smartSessionId)
            if (candidates.isEmpty()) {
                publishState(STATE_ERROR, "فهرست آزمون هوشمند در دسترس نیست؛ دوباره تلاش کنید.", probeComplete = true)
                stopSelf(startId)
                return START_NOT_STICKY
            }
            smartProbeInProgress = true
            startAsForeground("در حال بررسی همهٔ سرورها؛ برای توقف لمس کنید.")
            publishState(STATE_TESTING, "در حال آزمون تمام کانفیگ‌های پاسخ‌گوی TCP؛ هنوز اتصالی آغاز نشده است.")
            commands.execute { startSmartTunnel(candidates, startId) }
            return START_NOT_STICKY
        }
        publishState(STATE_ERROR, "درخواست اتصال هوشمند معتبر نیست؛ اتصال انجام نشد.", probeComplete = true)
        stopSelf(startId)
        return START_NOT_STICKY
    }

    private fun startAsForeground(contentText: String = "در حال راه‌اندازی تونل VPN؛ برای توقف لمس کنید.") {
        val manager = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.createNotificationChannel(NotificationChannel(CHANNEL_ID, "V2Speed VPN", NotificationManager.IMPORTANCE_LOW))
        }
        val stopIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, V2SpeedVpnService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val openIntent = PendingIntent.getActivity(
            this,
            2,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        @Suppress("DEPRECATION")
        val notificationBuilder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            Notification.Builder(this)
        }
        val notification = notificationBuilder
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle("V2Speed")
            .setContentText(contentText)
            .setContentIntent(openIntent)
            .addAction(Notification.Action.Builder(android.graphics.drawable.Icon.createWithResource(this, android.R.drawable.ic_media_pause), "توقف VPN", stopIntent).build())
            .setOngoing(true)
            .build()
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun startSmartTunnel(candidates: List<SmartConnectSession.Candidate>, startId: Int) {
        val passing = mutableListOf<SmartProbeCandidate>()
        candidates.forEachIndexed { index, candidate ->
            if (stopRequested.get()) return
            if (index == 0 || (index + 1) % PROGRESS_UPDATE_INTERVAL == 0 || index + 1 == candidates.size) {
                publishState(
                    STATE_TESTING,
                    "در حال آزمون HTTPS از مسیر Xray: ${index + 1} از ${candidates.size} کانفیگ پاسخ‌گوی TCP…"
                )
            }
            val probePort = runCatching { allocateLoopbackPort() }.getOrNull()
            if (probePort == null) return@forEachIndexed
            val probeCore = XrayCoreAdapter { }
            try {
                val probeConfig = SubscriptionParser.toXrayConfig(
                    uriText = candidate.uri,
                    includeTun = false,
                    socksProbePort = probePort
                )
                // Upstream AndroidLibXrayLite documents 0 as “do not use TUN”.
                probeCore.start(probeConfig, 0)
                if (!waitForProbeListener(probePort)) return@forEachIndexed
                val (instagram, youtube) = probeServices(probePort)
                if (instagram != null && youtube != null) {
                    passing += SmartProbeCandidate(
                        nodeId = candidate.nodeId,
                        uri = candidate.uri,
                        endpointLatencyMs = candidate.endpointLatencyMs,
                        instagramLatencyMs = instagram.latencyMs,
                        youtubeLatencyMs = youtube.latencyMs
                    )
                }
            } catch (_: Exception) {
                // Never log or show credential-bearing config details.
            } finally {
                runCatching { probeCore.stop() }
            }
        }
        if (stopRequested.get()) return

        // Do not establish a VPN connection until every TCP-responsive candidate has completed both HTTPS checks.
        val ranked = SmartServiceRanking.rank(passing)
        val remaining = ranked.toMutableList()
        smartProbeInProgress = false
        publishState(STATE_CONNECTING, "آزمون همهٔ کانفیگ‌ها کامل شد؛ ${ranked.size} سرور هر دو سرویس را پاسخ دادند. در حال تلاش برای بهترین رتبه…", rankedCandidates = remaining)
        startAsForeground("آزمون‌ها کامل شد؛ در حال برقراری تونل VPN.")
        var lastFailure: String? = null
        for (candidate in ranked) {
            if (stopRequested.get()) return
            lastFailure = establishTunnel(candidate.uri)
            if (lastFailure == null) {
                val message = "همهٔ ${candidates.size} کانفیگ پاسخ‌گوی TCP آزموده شدند. آزمون HTTPS سبک موفق: Instagram ${candidate.instagramLatencyMs} ms، YouTube ${candidate.youtubeLatencyMs} ms؛ پینگ TCP ${candidate.endpointLatencyMs} ms. سرعت و پایداری استریم تضمین نمی‌شود و مسیر TUN مستقل تأیید نشده است."
                startAsForeground("هستهٔ VPN فعال است؛ آزمون HTTPS هر دو سرویس موفق بود.")
                publishState(STATE_CONNECTED, message, nodeId = candidate.nodeId, rankedCandidates = remaining)
                announceConnected()
                return
            }
            remaining.removeAll { it.nodeId == candidate.nodeId }
            publishState(
                STATE_CONNECTING,
                "یک کانفیگ آزمون HTTPS را گذراند اما نتوانست تونل Android را شروع کند؛ از رتبهٔ بعدی تلاش می‌شود.",
                rankedCandidates = remaining
            )
        }

        val message = if (ranked.isEmpty()) {
            "همهٔ ${candidates.size} کانفیگ پاسخ‌گوی TCP آزموده شدند، اما هیچ‌کدام آزمون HTTPS هر دو سرویس را نگذرانْدند. هیچ اتصالی برقرار نشد."
        } else {
            "آزمون HTTPS هر دو سرویس موفق بود، اما هیچ‌یک از ${ranked.size} کانفیگ نتوانست تونل Android را شروع کند. ${lastFailure.orEmpty()}"
        }
        publishState(STATE_ERROR, message, rankedCandidates = remaining, probeComplete = true)
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf(startId)
    }

    private fun establishTunnel(uri: String): String? {
        var pfd: ParcelFileDescriptor? = null
        var engine: XrayCoreAdapter? = null
        return try {
            val config = SubscriptionParser.toXrayConfig(uri)
            pfd = Builder()
                .setSession("V2Speed")
                .setMtu(1500)
                .addAddress("10.88.0.1", 30)
                .addAddress("fd88::1", 126)
                .addRoute("0.0.0.0", 0)
                .addRoute("::", 0)
                .addDnsServer("1.1.1.1")
                .addDnsServer("2606:4700:4700::1111")
                .addDisallowedApplication(packageName)
                .establish() ?: error("Android اجازهٔ ساخت VPN را نداد.")
            engine = XrayCoreAdapter { }
            engine.start(config, pfd.fd)
            tunnel = pfd
            core = engine
            null
        } catch (error: Exception) {
            runCatching { engine?.stop() }
            runCatching { pfd?.close() }
            safeMessage(error)
        }
    }

    private fun allocateLoopbackPort(): Int = ServerSocket(0, 1, InetAddress.getByName("127.0.0.1")).use { it.localPort }

    private fun waitForProbeListener(port: Int): Boolean {
        val deadline = System.nanoTime() + PROBE_LISTENER_TIMEOUT_MS * 1_000_000L
        while (!stopRequested.get() && System.nanoTime() < deadline) {
            try {
                Socket().use { it.connect(InetSocketAddress("127.0.0.1", port), 250) }
                return true
            } catch (_: Exception) {
                Thread.sleep(PROBE_LISTENER_RETRY_MS)
            }
        }
        return false
    }

    private fun probeServices(port: Int): Pair<ServiceEndpointProbe.Result?, ServiceEndpointProbe.Result?> {
        val executor = Executors.newFixedThreadPool(2)
        return try {
            val instagram = executor.submit(Callable { ServiceEndpointProbe.instagram(port) })
            val youtube = executor.submit(Callable { ServiceEndpointProbe.youtube(port) })
            instagram.get(SERVICE_PROBE_DEADLINE_SECONDS, TimeUnit.SECONDS) to
                youtube.get(SERVICE_PROBE_DEADLINE_SECONDS, TimeUnit.SECONDS)
        } catch (_: Exception) {
            null to null
        } finally {
            executor.shutdownNow()
        }
    }

    private fun announceConnected() {
        if (!getSharedPreferences(MainActivity.PREFERENCES, MODE_PRIVATE)
                .getBoolean(MainActivity.KEY_VOICE_ANNOUNCEMENT, true)) return
        synchronized(speechLock) {
            queuedAnnouncement = CONNECTION_ANNOUNCEMENT
            speakQueuedAnnouncementLocked()
        }
    }

    private fun speakQueuedAnnouncementLocked() {
        if (!speechReady) return
        if (!getSharedPreferences(MainActivity.PREFERENCES, MODE_PRIVATE)
                .getBoolean(MainActivity.KEY_VOICE_ANNOUNCEMENT, true)) {
            queuedAnnouncement = null
            return
        }
        val message = queuedAnnouncement ?: return
        queuedAnnouncement = null
        val parameters = Bundle().apply {
            putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, 0.55f)
        }
        speech?.speak(message, TextToSpeech.QUEUE_FLUSH, parameters, "v2speed-vpn-connected")
    }

    private fun cancelAnnouncement() {
        synchronized(speechLock) {
            queuedAnnouncement = null
            speech?.stop()
        }
    }

    private fun stopTunnel() {
        stopRequested.set(true)
        val cancelledProbe = smartProbeInProgress
        smartProbeInProgress = false
        cancelAnnouncement()
        runCatching { core?.stop() }
        core = null
        closeTunnel()
        if (cancelledProbe) {
            publishState(STATE_DISCONNECTED, "آزمون لغو شد؛ هیچ کانفیگی به‌عنوان موفق رتبه‌بندی نشده است.", rankedCandidates = emptyList())
        } else {
            publishState(STATE_DISCONNECTED, "VPN متوقف شد.")
        }
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun closeTunnel() {
        runCatching { tunnel?.close() }
        tunnel = null
    }

    override fun onRevoke() {
        stopRequested.set(true)
        commands.execute { stopTunnel() }
        super.onRevoke()
    }

    override fun onDestroy() {
        stopRequested.set(true)
        commands.shutdownNow()
        cancelAnnouncement()
        runCatching { core?.stop() }
        core = null
        closeTunnel()
        synchronized(speechLock) {
            speechReady = false
            speech?.shutdown()
            speech = null
        }
        super.onDestroy()
    }

    private fun publishState(
        state: String,
        message: String,
        nodeId: String? = null,
        rankedCandidates: List<SmartProbeCandidate>? = null,
        probeComplete: Boolean = false
    ) {
        val broadcast = Intent(ACTION_STATE)
            .setPackage(packageName)
            .putExtra(EXTRA_STATE, state)
            .putExtra(EXTRA_MESSAGE, message)
        nodeId?.let { broadcast.putExtra(EXTRA_NODE_ID, it) }
        if (rankedCandidates != null) {
            broadcast.putExtra(EXTRA_PROBE_COMPLETE, true)
            broadcast.putStringArrayListExtra(EXTRA_RANKED_NODE_IDS, ArrayList(rankedCandidates.map { it.nodeId }))
            broadcast.putExtra(EXTRA_INSTAGRAM_RTT, rankedCandidates.map { it.instagramLatencyMs }.toLongArray())
            broadcast.putExtra(EXTRA_YOUTUBE_RTT, rankedCandidates.map { it.youtubeLatencyMs }.toLongArray())
            broadcast.putExtra(EXTRA_ENDPOINT_RTT, rankedCandidates.map { it.endpointLatencyMs }.toLongArray())
        } else if (probeComplete) {
            broadcast.putExtra(EXTRA_PROBE_COMPLETE, true)
            broadcast.putStringArrayListExtra(EXTRA_RANKED_NODE_IDS, arrayListOf())
            broadcast.putExtra(EXTRA_INSTAGRAM_RTT, longArrayOf())
            broadcast.putExtra(EXTRA_YOUTUBE_RTT, longArrayOf())
            broadcast.putExtra(EXTRA_ENDPOINT_RTT, longArrayOf())
        }
        sendBroadcast(broadcast)
    }

    private fun safeMessage(error: Exception): String = when (error) {
        is IllegalArgumentException -> error.message ?: "پیکربندی پشتیبانی نمی‌شود."
        else -> "اتصال شروع نشد. مجوز VPN، پیکربندی و دسترسی شبکه را بررسی کنید. (${error.javaClass.simpleName})"
    }

    companion object {
        const val ACTION_STATE = "app.v2speed.android.VPN_STATE"
        const val ACTION_STOP = "app.v2speed.android.STOP_VPN"
        const val EXTRA_SMART_SESSION_ID = "smart_session_id"
        const val EXTRA_RANKED_NODE_IDS = "ranked_node_ids"
        const val EXTRA_INSTAGRAM_RTT = "instagram_rtt_ms"
        const val EXTRA_YOUTUBE_RTT = "youtube_rtt_ms"
        const val EXTRA_ENDPOINT_RTT = "endpoint_rtt_ms"
        const val EXTRA_NODE_ID = "selected_node_id"
        const val EXTRA_PROBE_COMPLETE = "probe_complete"
        const val EXTRA_STATE = "vpn_state"
        const val EXTRA_MESSAGE = "vpn_message"
        const val STATE_TESTING = "testing"
        const val STATE_CONNECTING = "connecting"
        const val STATE_CONNECTED = "connected"
        const val STATE_DISCONNECTED = "disconnected"
        const val STATE_ERROR = "error"
        const val CONNECTION_ANNOUNCEMENT = "پاینده ایران، تا ابد جاوید شاه."
        private const val CHANNEL_ID = "v2speed-vpn"
        private const val NOTIFICATION_ID = 7201
        private const val PROBE_LISTENER_TIMEOUT_MS = 2_000L
        private const val PROBE_LISTENER_RETRY_MS = 100L
        private const val SERVICE_PROBE_DEADLINE_SECONDS = 5L
        private const val PROGRESS_UPDATE_INTERVAL = 5
    }
}
