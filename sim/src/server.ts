/**
 * CNC Stop Block — Mock ESP32 Server
 *
 * Starts an HTTP server on port 3001 that:
 *   - Serves the full REST API (mirrors firmware/src/WebAPI.cpp)
 *   - Broadcasts WebSocket status at 10 Hz (mirrors the ESP32 WebSocket)
 *   - Exposes /sim/* endpoints for injecting errors, tools, and test data
 *
 * Usage:
 *   npm run dev                     # watch mode (auto-restart on file change)
 *   npm start                       # run once
 *
 * Then in the ui/ project:
 *   VITE_PROXY_TARGET=http://localhost:3001 npm run dev
 */

import http from 'http'
import express from 'express'
import cors from 'cors'
import { WebSocketServer } from 'ws'
import { SimMachine } from './machine.js'
import { buildRouter } from './api.js'

const PORT = parseInt(process.env.SIM_PORT ?? '3001', 10)
const WS_TICK_MS = 100 // 10 Hz — mirrors WS_UPDATE_INTERVAL_MS in config.h

// ---------------------------------------------------------------------------
// Bootstrap
// ---------------------------------------------------------------------------
const sim = new SimMachine()
const app = express()

app.use(cors())
app.use(express.json())
app.use(buildRouter(sim))

const server = http.createServer(app)
const wss = new WebSocketServer({ server, path: '/ws' })

// ---------------------------------------------------------------------------
// WebSocket — broadcast status at 10 Hz, drop stale clients
// ---------------------------------------------------------------------------
wss.on('connection', (ws, req) => {
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
  console.log('  Useful commands:')
  console.log(`    Seed with sample data:  curl -X POST http://localhost:${PORT}/sim/seed`)
  console.log(`    Inject error:           curl -X POST http://localhost:${PORT}/sim/inject/error -H 'Content-Type: application/json' -d '{"message":"Stall detected"}'`)
  console.log(`    Inject tool:            curl -X POST http://localhost:${PORT}/sim/inject/tool  -H 'Content-Type: application/json' -d '{"uid":"AABB1122","name":"Freud 10\\" 40T","kerf_mm":3.2}'`)
  console.log(`    Full reset:             curl -X POST http://localhost:${PORT}/sim/reset`)
  console.log('')
  console.log('  UI: VITE_PROXY_TARGET=http://localhost:3001 npm run dev (from ui/)')
  console.log('')
})
