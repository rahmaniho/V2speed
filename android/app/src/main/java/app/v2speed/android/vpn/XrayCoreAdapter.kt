package app.v2speed.android.vpn

import libv2ray.CoreCallbackHandler
import libv2ray.CoreController
import libv2ray.Libv2ray

/** Narrow typed adapter for the pinned AndroidLibXrayLite gomobile API. */
internal class XrayCoreAdapter(private val status: (String) -> Unit) {
    private var controller: CoreController? = null

    @Synchronized
    fun start(config: String, tunFd: Int) {
        check(controller == null) { "Xray is already running" }
        Libv2ray.initCoreEnv("", "")
        val callback = object : CoreCallbackHandler {
            override fun startup(): Int {
                status("Xray core startup callback")
                return 0
            }

            override fun shutdown(): Int {
                status("Xray core shutdown callback")
                return 0
            }

            override fun onEmitStatus(code: Int, message: String): Int {
                // Do not forward/log core text: external node configs may contain sensitive fields.
                status("Xray core status $code")
                return 0
            }
        }
        val instance = Libv2ray.newCoreController(callback)
        controller = instance
        try {
            instance.startLoop(config, tunFd)
        } catch (error: Exception) {
            controller = null
            throw error
        }
        status("Xray core started")
    }

    @Synchronized
    fun stop() {
        val instance = controller ?: return
        try {
            instance.stopLoop()
        } finally {
            controller = null
        }
    }
}
