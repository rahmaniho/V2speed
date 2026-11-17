package app.v2speed.android.vpn

import libv2ray.CoreCallbackHandler
import libv2ray.CoreController
import libv2ray.Libv2ray
import java.lang.reflect.Proxy

/** Typed lifecycle adapter for the pinned AndroidLibXrayLite gomobile binding. */
internal class XrayCoreAdapter(private val status: (String) -> Unit) {
    private var controller: CoreController? = null

    @Synchronized
    fun start(config: String, tunFd: Int) {
        check(controller == null) { "Xray is already running" }
        Libv2ray.initCoreEnv("", "")
        val callback = createCallback()
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

    private fun createCallback(): CoreCallbackHandler {
        val handler = java.lang.reflect.InvocationHandler { proxy, method, args ->
            when {
                method.name.equals("startup", ignoreCase = true) -> {
                    status("Xray core startup callback")
                    zeroValue(method.returnType)
                }
                method.name.equals("shutdown", ignoreCase = true) -> {
                    status("Xray core shutdown callback")
                    zeroValue(method.returnType)
                }
                method.name.contains("emitstatus", ignoreCase = true) -> {
                    // Do not forward/log core text: external node configs may contain sensitive fields.
                    status("Xray core status callback")
                    zeroValue(method.returnType)
                }
                method.name == "toString" -> "V2Speed Xray callback"
                method.name == "hashCode" -> System.identityHashCode(proxy)
                method.name == "equals" -> proxy === args?.firstOrNull()
                else -> zeroValue(method.returnType)
            }
        }
        return Proxy.newProxyInstance(
            CoreCallbackHandler::class.java.classLoader,
            arrayOf(CoreCallbackHandler::class.java),
            handler
        ) as CoreCallbackHandler
    }

    private fun zeroValue(type: Class<*>): Any? = when (type) {
        java.lang.Void.TYPE -> null
        java.lang.Boolean.TYPE -> false
        java.lang.Byte.TYPE -> 0.toByte()
        java.lang.Short.TYPE -> 0.toShort()
        java.lang.Character.TYPE -> 0.toChar()
        java.lang.Integer.TYPE -> 0
        java.lang.Long.TYPE -> 0L
        java.lang.Float.TYPE -> 0f
        java.lang.Double.TYPE -> 0.0
        else -> null
    }
}
