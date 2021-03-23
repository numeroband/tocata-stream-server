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

  async find(email) {
    const res = await this.client.query('SELECT * FROM users WHERE email = $1::text;', [email]);
    return (res.rowCount === 0) ? null : res.rows[0];
  }

  end() {
    this.client.end();
  }
};

module.exports.Users = Users;
