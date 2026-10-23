# Launcher feed (server announcements and player count)

The launcher's Play page shows the joined server's announcements, and the top
bar shows how many players are on it. Both come from one route on the server's
backend, next to the `/fol/health` route the relay already uses.

## Route

```
GET http://SERVER:3551/fol/launcher
Host: 127.0.0.1:3551
```

The launcher sends the loopback `Host` header for the same reason
`integrations/lawin/relay.cjs` does for `/fol/health`: the backend answers only
for the Host the game uses. The launcher polls every 30 seconds while a valid
server address is set.

## Response

`200`, `Content-Type: application/json`, at most 256 KB:

```json
{
  "name": "Nocto's server",
  "players": {"online": 7, "max": 100},
  "announcements": [
    {
      "title": "Squad night this Friday at 20:00",
      "date": "2026-10-22",
      "url": "https://example.com/squad",
      "image": "https://example.com/squad.jpg"
    },
    {"title": "Server restart tonight at 23:00", "date": "2026-10-21"}
  ]
}
```

| Field | Required | Notes |
| --- | --- | --- |
| `name` | no | Shown in tooltips instead of the address; first 40 characters. |
| `players.online` | no | Players currently on the server (for example connected XMPP clients). Without it the chip says *Server online*. |
| `players.max` | no | Shown as `online/max` when greater than 0. |
| `announcements` | no | Newest first. The first one is the banner, the next four are rows. At most 20 are read. |
| `title` | yes | Plain text, first 120 characters. Markup is shown as typed. |
| `date` | no | ISO date (`YYYY-MM-DD`), shown as *22 Oct*. |
| `url` | no | `http`/`https` only; opens in the browser when clicked. Other schemes are ignored. |
| `image` | no | `http`/`https` banner image, about 16:9, under 4 MB. Only the first announcement's image is used. |

## Launcher behaviour

| Server answer | Player chip | Announcements card |
| --- | --- | --- |
| `200` with JSON | *7/100 PLAYING* (or *SERVER ONLINE*) | The posts, or *No announcements right now.* |
| Any other HTTP status (route not added yet) | *SERVER ONLINE* | *This server doesn't post announcements yet.* |
| No answer within 4 s | *SERVER OFFLINE* | *Can't reach the server right now.* |

The card's **Updates** tab is independent of the server: it lists Project
Tux's merged pull requests from the GitHub API
(`api.github.com/repos/Zxre0/Project-Tux/pulls`), fetched once at start-up.
