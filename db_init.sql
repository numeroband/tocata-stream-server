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

CREATE TABLE streams (
	id serial PRIMARY KEY,
	name VARCHAR ( 100 ) NOT NULL,
	user_id INT NOT NULL,
	CONSTRAINT fk_user
		FOREIGN KEY(user_id) 
		REFERENCES users(id)
		ON DELETE CASCADE
);

INSERT INTO 
    users (name, email, password, created_on)
VALUES
    ('Guest', 'guest@tocata.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp);

INSERT INTO 
    streams (name, user_id)
VALUES
    ('Guest Stream 1', 1),
    ('Guest Stream 2', 1),
    ('Guest Stream 3', 1),
    ('Guest Stream 4', 1);
