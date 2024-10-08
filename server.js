const WebSocketServer = require("ws").Server;
const fs = require("fs");
const http = require("http");
const https = require("https");
const bcrypt = require('bcrypt');
const express = require("express");
const bodyParser = require("body-parser");
const { userByEmail, userById, updateUser, pong, leaveSession, handleMessage } = require("./session");
const session = require('express-session');
const { body, validationResult } = require("express-validator");
const serveIndex = require('serve-index')

const port = process.env.PORT || 3000
const app = express();
const certs = process.env.CERTS_DIR;

app.use(session({
  secret: 'no one knows',
  resave: false,
  saveUninitialized: true
}))
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
  extended: true
}));
app.use(express.static('static'));

// Se indica el directorio donde se almacenarán las plantillas 
app.set('views', './views');

// Se indica el motor del plantillas a utilizar
app.set('view engine', 'pug');

app.get('/', (req, res) => res.redirect('/profile'))

app.get('/login', (req, res) => {
  return res.render('login');
});

app.post('/login',
  body('email').notEmpty().withMessage('Invalid email'),
  body('password').notEmpty().withMessage('Invalid password'),
  body('email').custom(async (value, {req}) => {
    const user = await userByEmail(value);
    if (!user) {
      throw new Error('Invalid user');
    }
    const match = await bcrypt.compare(req.body.password, user.password);
    if (!match) {
      throw new Error('Invalid password');
    }
    req.session.user = user.id;
    return true;
  }),
  (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) {
      return res.render('login', {...req.body, errors: errors.array()});
    }    
    return res.redirect('/profile');
  }
);

const auth = (req, res, next) => {
  if (req.session && req.session.user) {
    next();
  } else {
    res.redirect('/login');
  }
};

app.get('/profile', auth, async (req, res) => {
  const user = await userById(req.session.user);
  res.render('profile', user);
});

app.post('/profile',
  body('name').notEmpty(),
  body('email').isEmail(),
  body('passwordConfirmation').custom((value, { req }) => {
    const password = req.body.password
    if (password && value !== password) {
      throw new Error('Password confirmation does not match password');
    }
    return true;
  }),
  async (req, res) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) {
      return res.render('profile', {...req.body, errors: errors.array()});
    }
    const {name, email, password} = req.body;
    const id = req.session.user;
    await updateUser({id, name, email, password});
    return res.redirect('/profile');
  },  
)

app.use('/downloads', [auth, express.static('storage/downloads')], serveIndex('storage/downloads'))

const config = (certs ? {
  key: fs.readFileSync(certs + '/key.pem'),
  cert: fs.readFileSync(certs + '/cert.pem'),
} : null)
const server = (config ? 
  https.createServer(config, app) :
  http.createServer(app))
server.listen(port)

console.log(`http${config ? 's' : ''} server listening on ${port}`)

const wss = new WebSocketServer({ server })
console.log("websocket server created")

wss.on("connection", ws => {
  console.log("websocket connection open");
  ws.on("message", msg => handleMessage(ws, msg));
  ws.on("close", _ => leaveSession(ws));
  ws.on("pong", _ => pong(ws));
});
 
