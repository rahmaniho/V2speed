package app.v2speed.android.vpn

import java.util.UUID

/** Keeps credential-bearing probe candidates in-process instead of placing them in Binder intents. */
internal object SmartConnectSession {
    data class Candidate(val nodeId: String, val uri: String, val endpointLatencyMs: Long)

    private var sessionId: String? = null
    private var candidates: List<Candidate> = emptyList()

    @Synchronized
    fun stage(items: List<Candidate>): String {
        val id = UUID.randomUUID().toString()
        sessionId = id
        candidates = items.toList()
        return id
    }

    @Synchronized
    fun take(id: String): List<Candidate> {
        if (sessionId != id) return emptyList()
        val result = candidates
        sessionId = null
        candidates = emptyList()
        return result
    }

    @Synchronized
    fun clear() {
        sessionId = null
        candidates = emptyList()
    }
}
