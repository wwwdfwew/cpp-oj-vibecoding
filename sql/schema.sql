-- SPEC §2.3 — cpp-oj-vibecoding schema
CREATE DATABASE IF NOT EXISTS oj DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE oj;

DROP TABLE IF EXISTS test_cases;
DROP TABLE IF EXISTS submissions;
DROP TABLE IF EXISTS sessions;
DROP TABLE IF EXISTS problems;
DROP TABLE IF EXISTS users;

CREATE TABLE users (
  id            INT PRIMARY KEY AUTO_INCREMENT,
  username      VARCHAR(64)  NOT NULL UNIQUE,
  password_hash VARCHAR(128) NOT NULL,
  salt          VARCHAR(32)  NOT NULL,
  role          ENUM('user','admin') NOT NULL DEFAULT 'user',
  created_at    TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE sessions (
  token       CHAR(64)    PRIMARY KEY,
  user_id     INT         NOT NULL,
  expires_at  TIMESTAMP   NOT NULL,
  created_at  TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_expires (expires_at),
  CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE problems (
  id               INT PRIMARY KEY AUTO_INCREMENT,
  title            VARCHAR(255) NOT NULL,
  description      MEDIUMTEXT   NOT NULL,
  input_format     TEXT,
  output_format    TEXT,
  time_limit_ms    INT          NOT NULL DEFAULT 1000,
  memory_limit_mb  INT          NOT NULL DEFAULT 128,
  created_at       TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at       TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE test_cases (
  id               INT PRIMARY KEY AUTO_INCREMENT,
  problem_id       INT          NOT NULL,
  input            MEDIUMTEXT   NOT NULL,
  expected_output  MEDIUMTEXT   NOT NULL,
  ord              INT          NOT NULL DEFAULT 0,
  CONSTRAINT fk_testcases_problem FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE,
  INDEX idx_problem (problem_id, ord)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
