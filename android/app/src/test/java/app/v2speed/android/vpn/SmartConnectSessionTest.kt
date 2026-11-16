package app.v2speed.android.vpn

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SmartConnectSessionTest {
    @Test
    fun stagedCandidatesAreTakenOnceAndKeptOutOfIntentPayloads() {
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
}
