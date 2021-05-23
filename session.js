const bcrypt = require('bcrypt');

const {Users} = require('./users');
const { v4: uuidv4 } = require('uuid');

const peers = new Map();
const sessions = new Map();
const users = new Users();
users.connect();

const STATUS_DISCONNECTED = 0
const STATUS_INVALID_USER = 1
const STATUS_INVALID_PASSWORD = 2
const STATUS_CONNECTION_FAILED = 3
const STATUS_CONNECTING = 4
const STATUS_CONNECTED = 5

const PING_INTERVAL = 50 * 1000
const PING_TIMEOUT = 5 * 1000
const DISCONNECTION_TIMEOUT = 5 * 60 * 60 * 1000 // 5 hours
const SESSION_TIMEOUT = 10 * 60 * 1000 // 10 minutes

module.exports.userByEmail = email => users.find(email);
module.exports.userById = id => users.get(id);
module.exports.updateUser = async user => {
  if (user.password) {
    user.password = await bcrypt.hash(user.password, 10);
  }
  await users.update(user);
}

function findPeer(ws) {
  for (const [peer_id, peer] of peers.entries()) {
    if (peer.ws === ws) {
      return peer;
    }
  }
  return null;
}

function findSession(username) {
  const now = new Date().getTime();
  const toDelete = [];
  let foundSession = null;
  for (const [sessionId, session] of sessions.entries()) {
    if ((now - session.lastMessageMs) > SESSION_TIMEOUT) {
      toDelete.push(session);
    } else if (!foundSession || (foundSession.lastMessageMs < session.lastMessageMs)) {
      foundSession = session;
    }
  }
  // TODO: Update db timestamp
  toDelete.forEach(session => {
    console.log(`Destroying session ${session.id} after inactivity of ${(now - session.lastMessageMs) / 60000} minutes. Duration: ${(session.lastMessageMs - session.startMs) / 60000} minutes`);
    session.listeners.forEach(ws => ws.close())
    sessions.delete(session.id)
  });

  if (foundSession) {
    foundSession.lastMessageMs = now;
    return foundSession;
  }

  const session = {
    id: uuidv4(), 
    startMs: now, 
    lastMessageMs: now,
    startedBy: username,
    name: 'Dead Notes',
    listeners:[]
  };
  // TODO: Save into db
  sessions.set(session.id, session);
  console.log(`${username} created new session ${session.id}`);
  return session;
}

async function login(ws, msg) {
  const {type, username, password} = msg;
  console.log('Login from', username);
  
  const peer = await users.find(username);
  if (!peer) {
    console.log(`Invalid username: '${username}'`);
    const status = STATUS_INVALID_USER;
    const response = JSON.stringify({type, status});
    ws.send(response);
    return;
  }

  const match = await bcrypt.compare(password, peer.password);
  if (!match) {
    console.log(`Invalid password for username '${username}'`);
    const status = STATUS_INVALID_PASSWORD;
    const response = JSON.stringify({type, status});
    ws.send(response);
    return;
  }

  const status = STATUS_CONNECTED;
  const session = findSession(username);
  const sessionId = session.id;
  const sender = peer.id;
  const name = peer.name;
  const peerWithWs = {...peer, ws, session};
  peerWithWs.pingInterval = setInterval(_ => ping(peerWithWs), PING_INTERVAL);
  peers.set(sender, peerWithWs);
  const response = JSON.stringify({type, sender, name, status, sessionId});
  ws.send(response);
  peerWithWs.connectionMs = new Date().getTime();
  users.updateConnection(peer.id);
}

function listen(ws, msg) {
  const session = sessions.get(msg.sessionId);
  if (!session) {
    ws.close();
    return;
  }
  session.listeners.push(ws);
  msg.startMs = session.startMs;
  msg.name = session.name;
  ws.send(JSON.stringify(msg));
}

function ping(peer) {
  const connectedMs = new Date().getTime() - peer.connectionMs;
  if (connectedMs > DISCONNECTION_TIMEOUT) {
    console.log(`Disconnecting ${peer.name} connected ${connectedMs / (60 * 1000)} minutes`)    
    peer.ws.close();
    return;
  }

  peer.pingTimeout = setTimeout(_ => {
    peer.ws.close();
  }, PING_TIMEOUT);

  console.log(`Sending ping to ${peer.name}`)
  peer.ws.ping();
}

module.exports.handleMessage = (ws, msg) => {
  if (typeof(msg) === 'string') {
    handleJson(ws, JSON.parse(msg));
  } else {
    handleBinary(ws, msg);
  }
}

function handleBinary(ws, msg) {
  const peer = findPeer(ws);
  if (!peer) {
    ws.close();
    return;  
  }
  peer.session.listeners.forEach(listener => listener.send(msg));
}

function handleJson(ws, msg) {
    const {type, dst} = msg;
  if (type == 'Login') {
    return login(ws, msg);
  }

  if (type == 'Listen') {
    return listen(ws, msg);
  }

  const peer = findPeer(ws);
  if (!peer) {
    ws.close();
    return;  
  }

  peer.session.lastMessageMs = new Date().getTime();

  const sender = peer.id;
  const name = peer.name;

  const newMsg = JSON.stringify({sender, name, ...msg});

  if (dst) {
    const dst_peer = peers.get(dst);
    if (dst_peer) {
      console.log(`Sending message from ${name} to ${dst_peer.name}`, newMsg);
      dst_peer.ws.send(newMsg);  
    } else {
      console.err(`Unknown dst peer ${dst}`);
    }
  } else {
    console.log(`Broadcasting message from ${name}`, newMsg);
    peers.forEach(p => ws !== p.ws && p.ws.send(newMsg));
  }
}

module.exports.pong = ws => {
  console.log('Pong received');
  const peer = findPeer(ws);
  if (!peer) {
    return;
  }
  peer.session.lastMessageMs = new Date().getTime();
  peer.pingTimeout && clearTimeout(peer.pingTimeout);
  peer.pingTimeout = null;
}

module.exports.leaveSession = ws => {
  for (const session of sessions.values()) {
    session.listeners = session.listeners.filter(sessionWs => sessionWs !== ws);
  }
  
  const peer = findPeer(ws);
  if (!peer) {
    return;
  }
  peer.session.lastMessageMs = new Date().getTime();
  peer.pingTimeout && clearTimeout(peer.pingTimeout);
  peer.pingInterval && clearInterval(peer.pingInterval);
  const sender = peer.id;
  console.log('Disconnected', peer.name);
  peers.delete(sender);
  const type = 'Bye';
  const byeMsg = JSON.stringify({sender, type});
  for (const p of peers.values()) {
    p.ws.send(byeMsg);
  }
  users.updateDisconnection(peer.id);
}
