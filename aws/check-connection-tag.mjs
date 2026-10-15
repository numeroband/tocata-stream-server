// Checks that the signaling server tags relayed messages and Byes with the
// sender's `connection`, and that a login's Hello and Bye carry the same one.
//
//   A_USER=... A_PASS=... B_USER=... B_PASS=... node check-connection-tag.mjs
//
// A and B log in; B says Hello and then closes. A must see B's Hello and Bye,
// both tagged with one connection id. Other users logged in at the time also
// receive the Hello and Bye, as for any login.

const URL = 'wss://f9msb1bso8.execute-api.eu-south-2.amazonaws.com/production/';
const STATUS_CONNECTED = 5;
const TIMEOUT_MS = 20000;

const env = name => {
  const value = process.env[name];
  if (!value) {
    console.error(`Set ${name}`);
    process.exit(2);
  }
  return value;
};

function login(username, password) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(URL);
    const messages = [];
    const waiters = [];
    ws.onmessage = event => {
      const msg = JSON.parse(event.data);
      messages.push(msg);
      for (const waiter of waiters.splice(0)) waiter();
    };
    ws.onerror = () => reject(new Error(`${username}: socket error`));
    ws.onopen = () => ws.send(JSON.stringify({type: 'Login', username, password}));
    // Resolves with the first message (seen or future) that matches.
    const next = predicate => new Promise((done, fail) => {
      const timer = setTimeout(() => fail(new Error('timed out')), TIMEOUT_MS);
      const check = () => {
        const msg = messages.find(predicate);
        if (msg) {
          clearTimeout(timer);
          done(msg);
        } else {
          waiters.push(check);
        }
      };
      check();
    });
    next(msg => msg.type === 'Login').then(msg => {
      if (msg.status !== STATUS_CONNECTED) {
        reject(new Error(`${username}: login status ${msg.status}`));
      } else {
        resolve({ws, id: msg.sender, next});
      }
    }, reject);
  });
}

const a = await login(env('A_USER'), env('A_PASS'));
const b = await login(env('B_USER'), env('B_PASS'));
console.log(`Logged in: A = ${a.id}, B = ${b.id}`);

b.ws.send(JSON.stringify({type: 'Hello', name: 'connection-tag-check', buses: []}));
const hello = await a.next(msg => msg.type === 'Hello' && msg.sender === b.id);
console.log('A got Hello:', JSON.stringify(hello));

b.ws.close();
const bye = await a.next(msg => msg.type === 'Bye' && msg.sender === b.id);
console.log('A got Bye:  ', JSON.stringify(bye));
a.ws.close();

const ok = typeof hello.connection === 'string' && hello.connection.length > 0 &&
           bye.connection === hello.connection;
console.log(ok ? 'PASS: Hello and Bye carry the same connection'
               : 'FAIL: connection missing or different');
process.exit(ok ? 0 : 1);
