package app.v2speed.android.vpn

import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.nio.charset.StandardCharsets
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory

/** Performs a tiny HTTPS request through Xray's loopback-only SOCKS inbound. */
internal object ServiceEndpointProbe {
    data class Result(val latencyMs: Long, val httpStatus: Int)

    fun instagram(socksPort: Int, timeoutMs: Int = 3_500): Result? =
        probe("www.instagram.com", "/robots.txt", socksPort, timeoutMs)

    fun youtube(socksPort: Int, timeoutMs: Int = 3_500): Result? =
        probe("www.youtube.com", "/generate_204", socksPort, timeoutMs)

    private fun probe(host: String, path: String, socksPort: Int, timeoutMs: Int): Result? {
        if (socksPort !in 1..65_535 || timeoutMs !in 500..8_000) return null
        val start = System.nanoTime()
        return try {
            Socket().use { tunnelSocket ->
                tunnelSocket.connect(InetSocketAddress(InetAddress.getByName("127.0.0.1"), socksPort), 900)
                tunnelSocket.soTimeout = timeoutMs
                negotiateSocks5(tunnelSocket, host, 443)

                val tls = (SSLSocketFactory.getDefault() as SSLSocketFactory)
                    .createSocket(tunnelSocket, host, 443, false) as SSLSocket
                tls.use { secureSocket ->
                    secureSocket.soTimeout = timeoutMs
                    val parameters = secureSocket.sslParameters
                    parameters.endpointIdentificationAlgorithm = "HTTPS"
                    secureSocket.sslParameters = parameters
                    secureSocket.startHandshake()
                    val request = "GET $path HTTP/1.1\r\nHost: $host\r\nUser-Agent: V2Speed-Connectivity-Check\r\nConnection: close\r\n\r\n"
                    secureSocket.outputStream.write(request.toByteArray(StandardCharsets.US_ASCII))
                    secureSocket.outputStream.flush()
                    val statusLine = BufferedReader(
                        InputStreamReader(secureSocket.inputStream, StandardCharsets.US_ASCII)
                    ).readLine()
                    val status = statusLine?.split(' ')?.getOrNull(1)?.toIntOrNull() ?: return null
                    if (status !in 200..399) return null
                    Result(((System.nanoTime() - start) / 1_000_000L).coerceAtLeast(1L), status)
                }
            }
        } catch (_: Exception) {
            null
        }
    }

    private fun negotiateSocks5(socket: Socket, host: String, port: Int) {
        val hostBytes = host.toByteArray(StandardCharsets.US_ASCII)
        require(hostBytes.size in 1..255)
        val output = socket.getOutputStream()
        val input = socket.getInputStream()
        output.write(byteArrayOf(0x05, 0x01, 0x00))
        output.flush()
        require(readByte(input) == 0x05 && readByte(input) == 0x00) { "SOCKS auth negotiation failed" }
        output.write(byteArrayOf(0x05, 0x01, 0x00, 0x03, hostBytes.size.toByte()))
        output.write(hostBytes)
        output.write(byteArrayOf((port shr 8).toByte(), port.toByte()))
        output.flush()
        require(readByte(input) == 0x05) { "Invalid SOCKS response" }
        val reply = readByte(input)
        readByte(input) // reserved
        val addressType = readByte(input)
        require(reply == 0x00) { "SOCKS connect failed" }
        val addressBytes = when (addressType) {
            0x01 -> 4
            0x03 -> readByte(input)
            0x04 -> 16
            else -> error("Unsupported SOCKS address type")
        }
        repeat(addressBytes + 2) { readByte(input) }
    }

    private fun readByte(input: InputStream): Int {
        val value = input.read()
        check(value >= 0) { "Unexpected end of SOCKS response" }
        return value
    }
}
