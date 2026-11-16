package app.v2speed.android.data

/** A candidate is eligible only when both real HTTPS checks returned acceptable responses. */
data class SmartProbeCandidate(
    val nodeId: String,
    val uri: String,
    val endpointLatencyMs: Long,
    val instagramLatencyMs: Long,
    val youtubeLatencyMs: Long
) {
    val worstServiceLatencyMs: Long
        get() = maxOf(instagramLatencyMs, youtubeLatencyMs)
    val serviceAverageMs: Double
        get() = (instagramLatencyMs + youtubeLatencyMs) / 2.0
}

object SmartServiceRanking {
    fun rank(candidates: List<SmartProbeCandidate>): List<SmartProbeCandidate> = candidates.asSequence()
        .filter { it.nodeId.isNotBlank() && it.uri.isNotBlank() }
        .filter { it.endpointLatencyMs > 0 && it.instagramLatencyMs > 0 && it.youtubeLatencyMs > 0 }
        .distinctBy { it.nodeId }
        .sortedWith(compareBy<SmartProbeCandidate> { it.worstServiceLatencyMs }
            .thenBy { it.serviceAverageMs }
            .thenBy { it.endpointLatencyMs }
            .thenBy { it.nodeId })
        .toList()
}
