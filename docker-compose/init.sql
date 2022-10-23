DROP TABLE IF EXISTS streams;
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
	FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

INSERT INTO 
    users (name, email, password, created_on)
VALUES
    ('User1', 'user1@tocata.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp),
    ('User2', 'user2@tocata.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp),
    ('User3', 'user3@tocata.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp),
    ('User4', 'user4@tocata.com', '$2b$10$zHL3BTBPwVLx.Y8PK2aIuub4CVrEZwqcJu8KzJZHItOxOROZOCaVS', current_timestamp);

INSERT INTO 
    streams (name, user_id)
VALUES
    ('Stream1', 1),
    ('Stream2', 1),
    ('Stream3', 1),
    ('Stream1', 2),
    ('Stream2', 2),
    ('Stream3', 2),
    ('Stream1', 3),
    ('Stream2', 3),
    ('Stream3', 3),
    ('Stream1', 4),
    ('Stream2', 4),
    ('Stream3', 4);
