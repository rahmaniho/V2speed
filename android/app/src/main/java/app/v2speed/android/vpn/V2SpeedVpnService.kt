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
        val smartUris = intent?.getStringArrayListExtra(EXTRA_SMART_URIS)
        if (!smartUris.isNullOrEmpty()) {
            val ids = intent.getStringArrayListExtra(EXTRA_SMART_IDS).orEmpty()
            val endpointRtts = intent.getLongArrayExtra(EXTRA_ENDPOINT_RTT) ?: longArrayOf()
            if (smartUris.size != ids.size || smartUris.size != endpointRtts.size) {
                publishState(STATE_ERROR, "فهرست آزمون هوشمند ناقص است.")
                stopSelf(startId)
                return START_NOT_STICKY
            }
            startAsForeground()
            publishState(STATE_CONNECTING, "در حال آزمون امن و محدود چند کانفیگ…")
            commands.execute { startSmartTunnel(smartUris, ids, endpointRtts, startId) }
            return START_NOT_STICKY
        }
        val uri = intent?.getStringExtra(EXTRA_URI)
        if (uri.isNullOrBlank()) {
            publishState(STATE_ERROR, "پیکربندی انتخاب‌شده خالی است.")
            stopSelf(startId)
            return START_NOT_STICKY
        }
        startAsForeground()
        publishState(STATE_CONNECTING, "در حال ساخت تونل…")
        commands.execute { startTunnel(uri, startId) }
        return START_NOT_STICKY
    }

    private fun startAsForeground() {
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
            .setContentText("تونل VPN در حال اجراست؛ برای توقف لمس کنید.")
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

    private fun startTunnel(uri: String, startId: Int) {
        val failure = establishTunnel(uri)
        if (failure == null) {
            publishState(STATE_CONNECTED, "هستهٔ VPN فعال شد؛ مسیر TUN هنوز مستقل تأیید نشده است.")
            announceConnected()
        } else {
            publishState(STATE_ERROR, failure)
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf(startId)
        }
    }

    private fun startSmartTunnel(uris: List<String>, ids: List<String>, endpointRtts: LongArray, startId: Int) {
        val passing = mutableListOf<SmartProbeCandidate>()
        val rejectedIds = linkedSetOf<String>()

        uris.forEachIndexed { index, uri ->
            if (stopRequested.get()) return
            val nodeId = ids[index]
            val probePort = runCatching { allocateLoopbackPort() }.getOrNull()
            if (probePort == null) {
                rejectedIds += nodeId
                return@forEachIndexed
            }
            val probeCore = XrayCoreAdapter { }
            try {
                val probeConfig = SubscriptionParser.toXrayConfig(
                    uriText = uri,
                    includeTun = false,
                    socksProbePort = probePort
                )
                probeCore.start(probeConfig, 0)
                if (!waitForProbeListener(probePort)) {
                    rejectedIds += nodeId
                    return@forEachIndexed
                }
                val (instagram, youtube) = probeServices(probePort)
                if (instagram == null || youtube == null) {
                    rejectedIds += nodeId
                } else {
                    passing += SmartProbeCandidate(
                        nodeId = nodeId,
                        uri = uri,
                        endpointLatencyMs = endpointRtts[index],
                        instagramLatencyMs = instagram.latencyMs,
                        youtubeLatencyMs = youtube.latencyMs
                    )
                }
            } catch (_: Exception) {
                // Credentials/config details are intentionally not copied to logs or user-visible diagnostics.
                rejectedIds += nodeId
            } finally {
                runCatching { probeCore.stop() }
            }
        }
        if (stopRequested.get()) return

        val ranked = SmartServiceRanking.rank(passing)
        var lastFailure: String? = null
        for (candidate in ranked) {
            if (stopRequested.get()) return
            lastFailure = establishTunnel(candidate.uri)
            if (lastFailure == null) {
                val message = "آزمون HTTPS سبک موفق: Instagram ${candidate.instagramLatencyMs} ms، YouTube ${candidate.youtubeLatencyMs} ms؛ پینگ TCP سرور ${candidate.endpointLatencyMs} ms. سرعت و پایداری استریم تضمین نمی‌شود و مسیر TUN مستقل تأیید نشده است."
                publishState(STATE_CONNECTED, message, candidate.nodeId, rejectedIds.toList())
                announceConnected()
                return
            }
            rejectedIds += candidate.nodeId
        }

        val message = if (passing.isEmpty()) {
            "هیچ کانفیگی نتوانست آزمون HTTPS هر دو سرویس را از مسیر Xray کامل کند. هیچ کانفیگی انتخاب نشد."
        } else {
            "کانفیگ‌های آزموده‌شده نتوانستند تونل Android را شروع کنند. ${lastFailure.orEmpty()}"
        }
        publishState(STATE_ERROR, message, rejectedIds = rejectedIds.toList())
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
        cancelAnnouncement()
        runCatching { core?.stop() }
        core = null
        closeTunnel()
        publishState(STATE_DISCONNECTED, "VPN متوقف شد.")
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
        rejectedIds: List<String> = emptyList()
    ) {
        val broadcast = Intent(ACTION_STATE)
            .setPackage(packageName)
            .putExtra(EXTRA_STATE, state)
            .putExtra(EXTRA_MESSAGE, message)
        nodeId?.let { broadcast.putExtra(EXTRA_NODE_ID, it) }
        if (rejectedIds.isNotEmpty()) {
            broadcast.putStringArrayListExtra(EXTRA_REJECTED_IDS, ArrayList(rejectedIds))
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
        const val EXTRA_URI = "node_uri"
        const val EXTRA_SMART_URIS = "smart_node_uris"
        const val EXTRA_SMART_IDS = "smart_node_ids"
        const val EXTRA_ENDPOINT_RTT = "smart_endpoint_rtts"
        const val EXTRA_NODE_ID = "selected_node_id"
        const val EXTRA_REJECTED_IDS = "rejected_node_ids"
        const val EXTRA_STATE = "vpn_state"
        const val EXTRA_MESSAGE = "vpn_message"
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
    }
}
