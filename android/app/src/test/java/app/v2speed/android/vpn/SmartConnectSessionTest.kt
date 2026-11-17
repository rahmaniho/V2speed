package app.v2speed.android.vpn

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SmartConnectSessionTest {
    @Test
    fun stagedCandidatesAreTakenOnceAndSessionIsSingleUse() {
        SmartConnectSession.clear()
        val input = listOf(
            SmartConnectSession.Candidate("node-1", "vless://secret@edge.example:443", 42L),
            SmartConnectSession.Candidate("node-2", "trojan://secret@edge.example:8443", 55L)
        )
        val id = SmartConnectSession.stage(input)
        assertTrue(id.isNotBlank())
        assertEquals(input, SmartConnectSession.take(id))
        assertTrue(SmartConnectSession.take(id).isEmpty())
        SmartConnectSession.clear()
    }

    @Test
    fun latestProgressAndRankedResultsCanBeRestoredAfterActivityPause() {
        SmartConnectSession.clear()
        val progress = SmartConnectSession.recordStatus("testing", "Testing 2 of 3")
        val ranked = listOf(SmartConnectSession.RankedResult("node-1", 42L, 55L, 17L))
        val complete = SmartConnectSession.recordStatus("connected", "Probe checks passed", "node-1", ranked)
        assertTrue(complete.eventId > progress.eventId)
        assertEquals(complete, SmartConnectSession.latestStatus())
        assertEquals(ranked, SmartConnectSession.latestStatus()?.rankedResults)
        SmartConnectSession.clear()
        assertNull(SmartConnectSession.latestStatus())
    }
}
