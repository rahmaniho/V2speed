package app.v2speed.android.data

import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket

/** A short TCP-connect measurement to a server endpoint; this is not a tunnel or app-service test. */
data class EndpointMeasurement(val node: ServerNode, val latencyMs: Long)

object SmartEndpointRanking {
    fun rank(measurements: List<EndpointMeasurement>, limit: Int): List<EndpointMeasurement> {
        if (limit <= 0) return emptyList()
        return measurements.asSequence()
            .filter { it.latencyMs > 0 }
            .distinctBy { it.node.uri.substringBefore('#') }
            .sortedWith(compareBy<EndpointMeasurement> { it.latencyMs }.thenBy { it.node.id })
            .take(limit)
            .toList()
    }
}

object EndpointReachabilityProbe {
    fun measure(node: ServerNode, timeoutMs: Int = 1_400): EndpointMeasurement? {
        if (timeoutMs !in 250..5_000 || node.port !in 1..65_535) return null
        return try {
            val startedAt = System.nanoTime()
            val address = InetAddress.getAllByName(node.host)
                .firstOrNull(::isPubliclyRoutable) ?: return null
            Socket().use { socket ->
                socket.connect(InetSocketAddress(address, node.port), timeoutMs)
            }
            val elapsed = ((System.nanoTime() - startedAt) / 1_000_000L).coerceAtLeast(1L)
            EndpointMeasurement(node, elapsed)
        } catch (_: Exception) {
            null
        }
    }

    internal fun isPubliclyRoutable(address: InetAddress): Boolean =
        !address.isAnyLocalAddress &&
            !address.isLoopbackAddress &&
            !address.isLinkLocalAddress &&
            !address.isSiteLocalAddress &&
            !address.isMulticastAddress
}
