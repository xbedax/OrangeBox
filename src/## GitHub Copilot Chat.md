## GitHub Copilot Chat

- Extension: 0.53.1 (prod)
- VS Code: 1.125.1 (fcf604774b9f2674b473065736ee75077e256353)
- OS: win32 10.0.26200 x64
- GitHub Account: xsikulax-lang

## Network

User Settings:
```json
  "http.systemCertificatesNode": true,
  "github.copilot.advanced.debug.useElectronFetcher": true,
  "github.copilot.advanced.debug.useNodeFetcher": false,
  "github.copilot.advanced.debug.useNodeFetchFetcher": true
```

Environment Variables:
- NO_PROXY=127.0.0.1

Connecting to https://api.github.com:
- DNS ipv4 Lookup: 140.82.121.5 (128 ms)
- DNS ipv6 Lookup: Error (119 ms): getaddrinfo ENOTFOUND api.github.com
- Proxy URL: None (8 ms)
- Electron fetch (configured): HTTP 200 (270 ms)
- Node.js https: HTTP 200 (180 ms)
- Node.js fetch: HTTP 200 (165 ms)

Connecting to https://api.individual.githubcopilot.com/_ping:
- DNS ipv4 Lookup: 140.82.112.22 (34 ms)
- DNS ipv6 Lookup: Error (30 ms): getaddrinfo ENOTFOUND api.individual.githubcopilot.com
- Proxy URL: None (5 ms)
- Electron fetch (configured): HTTP 200 (456 ms)
- Node.js https: HTTP 200 (402 ms)
- Node.js fetch: HTTP 200 (431 ms)

Connecting to https://proxy.individual.githubcopilot.com/_ping:
- DNS ipv4 Lookup: 20.250.119.64 (64 ms)
- DNS ipv6 Lookup: Error (67 ms): getaddrinfo ENOTFOUND proxy.individual.githubcopilot.com
- Proxy URL: None (16 ms)
- Electron fetch (configured): HTTP 200 (166 ms)
- Node.js https: HTTP 200 (173 ms)
- Node.js fetch: HTTP 200 (174 ms)

Connecting to https://mobile.events.data.microsoft.com: HTTP 404 (148 ms)
Connecting to https://dc.services.visualstudio.com: HTTP 404 (251 ms)
Connecting to https://copilot-telemetry.githubusercontent.com/_ping: HTTP 200 (478 ms)
Connecting to https://telemetry.individual.githubcopilot.com/_ping: HTTP 200 (447 ms)
Connecting to https://default.exp-tas.com: HTTP 400 (187 ms)

Number of system certificates: 148

## Documentation

In corporate networks: [Troubleshooting firewall settings for GitHub Copilot](https://docs.github.com/en/copilot/troubleshooting-github-copilot/troubleshooting-firewall-settings-for-github-copilot).