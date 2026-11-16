package app.v2speed.android.vpn

import java.lang.reflect.Proxy

/** Thin adapter to the pinned gomobile binding; reflection keeps its generated Java API out of UI code. */
internal class XrayCoreAdapter(private val status: (String) -> Unit) {
    private val api = loadApiClass()
    private var controller: Any? = null

    fun start(config: String, tunFd: Int) {
        check(controller == null) { "Xray is already running" }
        runCatching {
            api.getMethod("initCoreEnv", String::class.java, String::class.java).invoke(null, "", "")
        }
        val callbackType = loadCallbackType()
        val callback = Proxy.newProxyInstance(callbackType.classLoader, arrayOf(callbackType)) { proxy, method, args ->
            when (method.name.lowercase()) {
                "startup", "shutdown" -> 0
                "onemitstatus" -> {
                    status("Xray status ${args?.getOrNull(0) ?: ""}")
                    0
                }
                "tostring" -> "V2Speed Xray callback"
                "hashcode" -> System.identityHashCode(proxy)
                "equals" -> proxy === args?.firstOrNull()
                else -> 0
            }
        }
        val factory = api.methods.firstOrNull { it.name.equals("newCoreController", ignoreCase = true) && it.parameterCount == 1 }
            ?: error("Pinned Xray binding has no CoreController factory")
        controller = factory.invoke(null, callback)
        val instance = controller ?: error("Could not create Xray controller")
        val startMethod = instance.javaClass.methods.firstOrNull {
            it.name.equals("startLoop", ignoreCase = true) && it.parameterCount == 2 && it.parameterTypes[0] == String::class.java
        } ?: error("Pinned Xray binding has no StartLoop(config, tunFd)")
        val fdArg: Any = when (startMethod.parameterTypes[1]) {
            java.lang.Long.TYPE, java.lang.Long::class.java -> tunFd.toLong()
            java.lang.Integer.TYPE, java.lang.Integer::class.java -> tunFd
            else -> error("Unsupported TUN descriptor type in Xray binding")
        }
        try {
            startMethod.invoke(instance, config, fdArg)
        } catch (error: java.lang.reflect.InvocationTargetException) {
            controller = null
            throw (error.targetException ?: error)
        }
        status("Core started")
    }

    fun stop() {
        val instance = controller ?: return
        try {
            instance.javaClass.methods.firstOrNull { it.name.equals("stopLoop", ignoreCase = true) && it.parameterCount == 0 }
                ?.invoke(instance)
        } finally {
            controller = null
        }
    }

    private fun loadApiClass(): Class<*> {
        val names = listOf("libv2ray.Libv2ray", "libv2ray.AndroidLibXrayLite")
        return names.firstNotNullOfOrNull { name -> runCatching { Class.forName(name) }.getOrNull() }
            ?: error("Pinned Xray AAR is missing its gomobile API")
    }

    private fun loadCallbackType(): Class<*> {
        val names = listOf("libv2ray.CoreCallbackHandler", "libv2ray.Libv2ray\$CoreCallbackHandler")
        return names.firstNotNullOfOrNull { name -> runCatching { Class.forName(name) }.getOrNull() }
            ?: error("Pinned Xray AAR is missing CoreCallbackHandler")
    }
}
