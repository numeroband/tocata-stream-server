const { Client } = require('pg');
const sqlite3 = require('sqlite3');
const { open } = require('sqlite');

const remoteUrl = {
  connectionString: process.env.DATABASE_URL,
  ssl: false
};

const localUrl = {
  connectionString: 'postgres://localhost/tocata-stream'
};

class Users {
  constructor() {
    this.client = new Client(process.env.DATABASE_URL ? remoteUrl : localUrl);    
  }

  async connect() {
    this.client.connect();
  }

  async get(id) {
    const res = await this.client.query('SELECT * FROM users WHERE id = $1;', [id]);
    return (res.rowCount === 0) ? null : res.rows[0];
  }

  async find(email) {
    const res = await this.client.query('SELECT * FROM users WHERE email = $1;', [email]);
    return (res.rowCount === 0) ? null : res.rows[0];
  }

  async update(user) {
    await user.password ? 
      this.client.query('UPDATE users SET name = $2, email = $3, password = $4 WHERE id = $1;', 
        [user.id, user.name, user.email, user.password]) :
        this.client.query('UPDATE users SET name = $2, email = $3 WHERE id = $1;',
        [user.id, user.name, user.email]);
  }

  async getStreams(userId) {
    const res = await this.client.query('SELECT * FROM streams WHERE user_id = $1;', [userId]);
    return res.rows;
  }

  updateConnection(id) {
    return this.client.query('UPDATE users SET last_connected = current_timestamp WHERE id = $1;', [id]);
  }

  updateDisconnection(id) {
    return this.client.query('UPDATE users SET last_disconnected = current_timestamp WHERE id = $1;', [id]);
  }

  end() {
    this.client.end();
  }
};

class UsersLite {
  constructor() {
    this.db = null
  }

  async connect() {
    if (this.db) { return }
    this.db = await open({
      filename: 'storage/db/tocata.db',
      driver: sqlite3.Database
    })
  }

  async get(id) {
    await this.connect()
    return this.db.get('SELECT * FROM users WHERE id = ?', id)
  }

  async find(email) {
    await this.connect()
    return this.db.get('SELECT * FROM users WHERE email = ?', email)
  }

  async update(user) {
    await this.connect()
    const values = {
      ':name': user.name,
      ':email': user.email,
      ':password' : user.password,
      ':id' : user.id,
    }
    await user.password ? 
      this.db.run('UPDATE users SET name = :name, email = :email, password = :password WHERE id = :id', values) :
        this.db.run('UPDATE users SET name = :name, email = :email WHERE id = :id', values);
  }

  async getStreams(userId) {
    await this.connect()
    return this.db.all('SELECT * FROM streams WHERE user_id = ?', userId);
  }

  async updateConnection(id) {
    await this.connect()
    return this.db.run('UPDATE users SET last_connected = current_timestamp WHERE id = ?', id);
  }

  async updateDisconnection(id) {
    await this.connect()
    return this.db.run('UPDATE users SET last_disconnected = current_timestamp WHERE id = ?', id);
  }

  async end() {
    const db = this.db;
    this.db = null;
    await db.close();
  }
};

module.exports.Users = Users;
module.exports.UsersLite = UsersLite;
