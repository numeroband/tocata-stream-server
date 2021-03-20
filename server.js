const WebSocketServer = require("ws").Server;
const http = require("http");
const express = require("express");
const bodyParser = require("body-parser");
const {leaveSession, handleMessage} = require("./session");

const port = process.env.PORT || 5000
const app = express();

app.use(bodyParser.json());

const server = http.createServer(app)
server.listen(port)

console.log("http server listening on %d", port)

const wss = new WebSocketServer({server})
console.log("websocket server created")

wss.on("connection", ws => {
  console.log("websocket connection open");
  ws.on("message", msg => handleMessage(ws, JSON.parse(msg)));
  ws.on("close", _ => leaveSession(ws));
});
