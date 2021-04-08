DROP TABLE IF EXISTS users;

CREATE TABLE users (
	id serial PRIMARY KEY,
	name VARCHAR ( 100 ) NOT NULL,
	email VARCHAR ( 255 ) UNIQUE NOT NULL,
	password VARCHAR ( 100 ) NOT NULL,
	created_on TIMESTAMP NOT NULL,
    last_connected TIMESTAMP, 
    last_disconnected TIMESTAMP
);

INSERT INTO 
    users (name, email, password, created_on)
VALUES
    ('Loren', 'lorenzo.soto@gmail.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp);
