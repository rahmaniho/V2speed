package app.v2speed.android.vpn

import java.util.UUID

/** Keeps credential-bearing probe candidates in-process instead of placing them in Binder intents. */
internal object SmartConnectSession {
    data class Candidate(val nodeId: String, val uri: String, val endpointLatencyMs: Long)
    data class RankedResult(val nodeId: String, val instagramMs: Long, val youtubeMs: Long, val endpointMs: Long)
    data class StatusSnapshot(
        val eventId: Long,
        val state: String,
        val message: String,
        val nodeId: String?,
        val rankedResults: List<RankedResult>?
    )

    private var sessionId: String? = null
    private var candidates: List<Candidate> = emptyList()
    private var eventId = 0L
    private var latestStatus: StatusSnapshot? = null

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
    fun recordStatus(
        state: String,
        message: String,
        nodeId: String? = null,
        rankedResults: List<RankedResult>? = null
    ): StatusSnapshot {
        val snapshot = StatusSnapshot(++eventId, state, message, nodeId, rankedResults?.toList())
        latestStatus = snapshot
        return snapshot
    }

    @Synchronized
    fun latestStatus(): StatusSnapshot? = latestStatus

    @Synchronized
    fun clear() {
        sessionId = null
        candidates = emptyList()
        latestStatus = null
    }
}
