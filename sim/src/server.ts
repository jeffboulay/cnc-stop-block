/**
 * CNC Stop Block — Mock ESP32 Server
 *
 * Starts an HTTP server on port 3001 that:
 *   - Serves the full REST API (mirrors firmware/src/WebAPI.cpp)
 *   - Broadcasts WebSocket status at 10 Hz (mirrors the ESP32 WebSocket)
 *   - Exposes /sim/* endpoints for injecting errors, tools, and test data
 *
 * Authentication
 * ──────────────
 * The sim mirrors the ESP32 bearer-token auth added in Phase 1.
 * On startup a token is printed to the console (or read from SIM_TOKEN env).
 * Pass it in the UI pairing screen to use the sim as a drop-in for real hardware.
 *
 * Usage:
 *   npm run dev                     # watch mode (auto-restart on file change)
 *   npm start                       # run once
 *
 * Then in the ui/ project:
 *   VITE_PROXY_TARGET=http://localhost:3001 npm run dev
 */

import http from 'http'
import crypto from 'crypto'
import express, { type Request, type Response, type NextFunction } from 'express'
import cors from 'cors'
import { WebSocketServer, type WebSocket } from 'ws'
import { SimMachine } from './machine.js'
import { buildRouter } from './api.js'

const PORT = parseInt(process.env.SIM_PORT ?? '3001', 10)
const WS_TICK_MS = 100 // 10 Hz — mirrors WS_UPDATE_INTERVAL_MS in config.h

// ---------------------------------------------------------------------------
// Auth token (mirrors firmware/src/WebAPI.cpp token generation)
// Set SIM_TOKEN env var to use a fixed token across restarts.
// ---------------------------------------------------------------------------
const SIM_TOKEN: string = process.env.SIM_TOKEN ?? crypto.randomBytes(32).toString('hex')

// ---------------------------------------------------------------------------
// Bootstrap
// ---------------------------------------------------------------------------
const sim = new SimMachine()
const app = express()

app.use(cors({ exposedHeaders: ['Authorization'] }))
app.use(express.json())

// ---------------------------------------------------------------------------
// Auth middleware — mirrors isAuthenticated() in WebAPI.cpp
// /sim/* endpoints are exempt (sim-only dev tooling).
// GET /api/status is exempt (mirrors AUTH_EXEMPT_STATUS).
// ---------------------------------------------------------------------------
app.use((req: Request, res: Response, next: NextFunction) => {
  // Always allow OPTIONS (CORS preflight) and sim-only endpoints
  if (req.method === 'OPTIONS' || req.path.startsWith('/sim/')) {
    return next()
  }
  // Mirror AUTH_EXEMPT_STATUS: allow GET /api/status without auth
  if (req.method === 'GET' && req.path === '/api/status') {
    return next()
  }
  const auth = req.headers['authorization'] ?? ''
  if (!auth.startsWith('Bearer ') || auth.slice(7) !== SIM_TOKEN) {
    res.status(401).json({ error: 'Unauthorized' })
    return
  }
  next()
})

app.use(buildRouter(sim))

const server = http.createServer(app)
const wss = new WebSocketServer({ server, path: '/ws' })

// ---------------------------------------------------------------------------
// WebSocket — validate token from ?token= query param (mirrors firmware)
// ---------------------------------------------------------------------------
wss.on('connection', (ws: WebSocket, req) => {
  const url = new URL(req.url ?? '/', `http://localhost:${PORT}`)
  const provided = url.searchParams.get('token') ?? ''

  if (provided !== SIM_TOKEN) {
    console.log('[WS]  Rejecting client — invalid or missing token')
    ws.close(1008, 'Unauthorized')
    return
  }

  const ip = req.socket.remoteAddress ?? 'unknown'
  console.log(`[WS]  Client connected  (${ip}) — ${wss.clients.size} total`)

  // Send immediate snapshot on connect (mirrors ESP32 WS_EVT_CONNECT handler)
  ws.send(JSON.stringify(sim.getStatus()))

  ws.on('close', () => {
    console.log(`[WS]  Client disconnected (${ip}) — ${wss.clients.size} total`)
  })
})

setInterval(() => {
  sim.tick()

  if (wss.clients.size === 0) return
  const payload = JSON.stringify(sim.getStatus())
  for (const client of wss.clients) {
    if (client.readyState === client.OPEN) {
      client.send(payload)
    }
  }
}, WS_TICK_MS)

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
server.listen(PORT, () => {
  console.log('')
  console.log('  CNC Stop Block — Mock ESP32')
  console.log('  ─────────────────────────────────────')
  console.log(`  REST API  →  http://localhost:${PORT}/api/status`)
  console.log(`  WebSocket →  ws://localhost:${PORT}/ws`)
  console.log(`  Sim tools →  http://localhost:${PORT}/sim/state`)
  console.log('')
  console.log('  *** SIM API TOKEN ***')
  console.log(`  ${SIM_TOKEN}`)
  console.log('  Enter this in the UI pairing screen (or set SIM_TOKEN env to fix it).')
  console.log('')
  console.log('  Useful commands (no auth required for /sim/* endpoints):')
  console.log(`    Seed with sample data:  curl -X POST http://localhost:${PORT}/sim/seed`)
  console.log(`    Inject error:           curl -X POST http://localhost:${PORT}/sim/inject/error -H 'Content-Type: application/json' -d '{"message":"Stall detected"}'`)
  console.log(`    Inject tool:            curl -X POST http://localhost:${PORT}/sim/inject/tool  -H 'Content-Type: application/json' -d '{"uid":"AABB1122","name":"Freud 10\\" 40T","kerf_mm":3.2}'`)
  console.log(`    Full reset:             curl -X POST http://localhost:${PORT}/sim/reset`)
  console.log('')
  console.log('  UI: VITE_PROXY_TARGET=http://localhost:3001 npm run dev (from ui/)')
  console.log('')
})
