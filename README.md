# DigitalColleague - A stupid bot, for twitch and discord

## Resources
- [Boost ASIO based IRC Client](https://github.com/KrzaQ/boost-asio-irc-client)
- [Discord Developer Portal](https://discord.com/developers/docs)
- [RFC 1459 - IRC Protocol](https://tools.ietf.org/html/rfc1459)

## Requirements
- Boost (Asio, SSL, Beast, JSON)
- OpenSSL
- SQLite3

## Example Config
```json
{
  "twitch": {
    "enabled": true,
    "host": "irc.chat.twitch.tv",
    "port": 6697,
    "nick": "botname",
    "pass": "oauth:oauth_token",
    "channels": [ "#somechannel" ]
  },
  "discord": {
    "enabled": true,
    "token": "discord bot token"
  },
  "console": {
    "enabled": true,
    "port": 6969
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

## Build
```
$ cmake -B build -S .
$ cmake --build build/
```

## Usage
```
$ ./digitalcolleague config.json bot.db
```
