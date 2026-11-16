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
import java.util.concurrent.Executors
import app.v2speed.android.MainActivity
import app.v2speed.android.data.SubscriptionParser

class V2SpeedVpnService : VpnService() {
    private var tunnel: ParcelFileDescriptor? = null
    private var core: XrayCoreAdapter? = null
    private val commands = Executors.newSingleThreadExecutor { task -> Thread(task, "v2speed-vpn-lifecycle") }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            commands.execute { stopTunnel() }
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
        try {
            val config = SubscriptionParser.toXrayConfig(uri)
            val pfd = Builder()
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
            tunnel = pfd
            val engine = XrayCoreAdapter { }
            core = engine
            engine.start(config, pfd.fd)
            publishState(STATE_CONNECTED, "تونل و هسته شروع شدند؛ دسترسی اینترنت هنوز مستقل تأیید نشده است.")
        } catch (error: Exception) {
            closeTunnel()
            publishState(STATE_ERROR, safeMessage(error))
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf(startId)
        }
    }

    private fun stopTunnel() {
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
        commands.execute { stopTunnel() }
        super.onRevoke()
    }

    override fun onDestroy() {
        commands.shutdownNow()
        runCatching { core?.stop() }
        core = null
        closeTunnel()
        super.onDestroy()
    }

    private fun publishState(state: String, message: String) {
        sendBroadcast(Intent(ACTION_STATE).setPackage(packageName).putExtra(EXTRA_STATE, state).putExtra(EXTRA_MESSAGE, message))
    }

    private fun safeMessage(error: Exception): String = when (error) {
        is IllegalArgumentException -> error.message ?: "پیکربندی پشتیبانی نمی‌شود."
        else -> "اتصال شروع نشد. مجوز VPN، پیکربندی و دسترسی شبکه را بررسی کنید. (${error.javaClass.simpleName})"
    }

    companion object {
        const val ACTION_STATE = "app.v2speed.android.VPN_STATE"
        const val ACTION_STOP = "app.v2speed.android.STOP_VPN"
        const val EXTRA_URI = "node_uri"
        const val EXTRA_STATE = "vpn_state"
        const val EXTRA_MESSAGE = "vpn_message"
        const val STATE_CONNECTING = "connecting"
        const val STATE_CONNECTED = "connected"
        const val STATE_DISCONNECTED = "disconnected"
        const val STATE_ERROR = "error"
        private const val CHANNEL_ID = "v2speed-vpn"
        private const val NOTIFICATION_ID = 7201
    }
}
