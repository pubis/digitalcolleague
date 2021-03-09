# DigitalColleague - A twitch chat bot

## Resources
- [Boost ASIO based IRC Client](https://github.com/KrzaQ/boost-asio-irc-client)

## Requirements
- Boost (Asio, SSL, JSON)
- OpenSSL
- SQLite3

## Config
```json
{
  "twitch": {
    "enabled": true,
    "host": "irc.chat.twitch.tv",
    "port": 6697,
    "nick": "botname",
    "pass": "oauth:oauth_token",
    "channels": [ "#somechannel" ]
  }
}
```

## Database
```sql
CREATE TABLE message (
  id INTEGER PRIMARY KEY,
  timestamp INTEGER,
  nick TEXT,
  channel TEXT,
  message TEXT
);
```
