#!/usr/bin/env python3
"""The egress proxy lets a job reach GitHub and Sigstore, and nothing else."""

import asyncio
import unittest

from egress_proxy import handle, host_allowed, parse_connect, public_address


class Rules(unittest.TestCase):
    def test_github_and_sigstore_pass(self):
        for host in ("github.com", "api.github.com", "codeload.github.com",
                     "objects.githubusercontent.com", "pipelines.actions.githubusercontent.com",
                     "productionresultssa0.blob.core.windows.net", "ghcr.io",
                     "fulcio.sigstore.dev", "rekor.sigstore.dev", "GitHub.com."):
            self.assertTrue(host_allowed(host), host)

    def test_everything_else_is_refused(self):
        for host in ("", "evilgithub.com", "github.com.evil.net", "githubusercontent.com.evil.net",
                     "example.com", "localhost", "portainer", "140.82.112.3", "10.0.0.1",
                     "[::1]", "git.hub.com", "github.com:443", "sigstore.dev.evil.net"):
            self.assertFalse(host_allowed(host), host)

    def test_connect_line(self):
        self.assertEqual(parse_connect(b"CONNECT api.github.com:443 HTTP/1.1"), ("api.github.com", 443))
        for line in (b"GET http://github.com/ HTTP/1.1", b"CONNECT github.com HTTP/1.1",
                     b"CONNECT :443 HTTP/1.1", b"CONNECT github.com:https HTTP/1.1",
                     b"CONNECT github.com:443", b"CONNECT github.com:443 SPDY/3"):
            self.assertIsNone(parse_connect(line), line)

    def test_only_public_addresses(self):
        for address in ("140.82.112.3", "2606:50c0:8000::153"):
            self.assertTrue(public_address(address), address)
        for address in ("10.1.2.3", "172.17.0.1", "192.168.1.8", "100.88.3.84", "127.0.0.1",
                        "169.254.169.254", "0.0.0.0", "224.0.0.1", "::1", "fe80::1%eth0",
                        "fd7a:115c:a1e0::1"):
            self.assertFalse(public_address(address), address)


class Refusals(unittest.TestCase):
    """End to end over a real socket; none of these needs the internet."""

    def exchange(self, request: bytes) -> bytes:
        async def run() -> bytes:
            server = await asyncio.start_server(handle, "127.0.0.1", 0)
            port = server.sockets[0].getsockname()[1]
            async with server:
                reader, writer = await asyncio.open_connection("127.0.0.1", port)
                writer.write(request)
                await writer.drain()
                reply = await asyncio.wait_for(reader.read(), 10)
                writer.close()
                return reply
        return asyncio.run(run())

    def test_plain_http_is_refused(self):
        self.assertTrue(self.exchange(b"GET http://github.com/ HTTP/1.1\r\nHost: github.com\r\n\r\n")
                        .startswith(b"HTTP/1.1 405"))

    def test_other_hosts_and_ports_are_refused(self):
        for target in (b"example.com:443", b"github.com:22", b"127.0.0.1:443", b"localhost:443"):
            reply = self.exchange(b"CONNECT " + target + b" HTTP/1.1\r\nHost: x\r\n\r\n")
            self.assertTrue(reply.startswith(b"HTTP/1.1 403"), (target, reply))


if __name__ == "__main__":
    unittest.main()
