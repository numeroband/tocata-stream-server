const jwt = require('jsonwebtoken');

const peers = new Map();
const secret = 'TocataStreamSecret'

function findPeer(ws) {
  for (const [peer, peerWs] of peers.entries()) {
    if (peerWs === ws) {
      return peer;
    }
  }
}

function verifyToken(ws, token) {
  const decoded = jwt.verify(token, secret);
  if (!decoded) {
    console.log('Invalid token', token);
    return null;
  }
  peers.set(decoded.sub, ws);
  return decoded.sub;
}

module.exports.joinSession = (username, password) => {
  const token = jwt.sign({ sub: username }, secret, { expiresIn: '1d' });
  console.log('sending', token);
  return token;
}

module.exports.handleMessage = (ws, msg) => {
  let {token, dst} = msg;
  const sender = findPeer(ws) || verifyToken(ws, token);
  
  if (!sender) {
    ws.close();
    return;  
  }

  const removeToken = msg => {
    const {token, ...msgWithoutToken} = msg;
    return msgWithoutToken;
  }
  const newMsg = JSON.stringify({sender, ...removeToken(msg)});

  if (dst) {
    console.log(`Sending message from ${sender} to ${dst}`, newMsg);
    peers.has(dst) && peers.get(dst).send(newMsg);
  } else {
    console.log(`Broadcasting message from ${sender}`, newMsg);
    peers.forEach(peerWs => ws !== peerWs && peerWs.send(newMsg));
  }
}

module.exports.leaveSession = ws => {
  const sender = findPeer(ws);
  if (!sender) {
    return;
  }
  console.log('Disconnected', sender);
  peers.delete(sender);
  const type = 'Bye';
  const byeMsg = JSON.stringify({sender, type});
  for (const peerWs of peers.values()) {
    peerWs.send(byeMsg);
  }
}
