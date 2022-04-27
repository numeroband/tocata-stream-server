const { Client } = require('pg');

const remoteUrl = {
  connectionString: process.env.DATABASE_URL,
  ssl: {
    rejectUnauthorized: false
  }
};

const localUrl = {
  connectionString: 'postgres://localhost/tocata-stream'
};

class Users {
  constructor() {
    this.client = new Client(process.env.DATABASE_URL ? remoteUrl : localUrl);    
  }

  connect() {
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

module.exports.Users = Users;
