var syncData = {}
var currentRoom = ''
var authToken = ''

// Setting functions
function getSettings() {
    var s = localStorage.getItem("matrix_settings");
    if (!s) return null;
    return JSON.parse(s);
}

function getHostServer() {
    var settings = getSettings();
    if (!settings) return null;
    var hostserver = settings['hostserver'];
    return hostserver;
}

// Api functions

function login(callback) {

    settings = getSettings();
    if (!settings) {
        callback(null);
        return;
    }

    var hostserver = settings['hostserver'];
    var user = settings['user'];
    var pass = settings['pass'];

    if (!hostserver || !user || !pass) {
        console.log("Missing config values");
        callback(null);
        return;
    }


    // LOGIN

    // Make https request
    var xhr = new XMLHttpRequest();
    xhr.open("POST", hostserver + "/_matrix/client/v3/login");

    var data = {
        'type': 'm.login.password',
        'identifier': {
            'type': 'm.id.user',
            'user': user
        },
        'password': pass
    }

    xhr.onload = function () {
        var responseText = JSON.parse(xhr.responseText);
        console.log(xhr.responseText);

        const token = responseText["access_token"] || null;
        authToken = token;
        callback(token);
    };

    xhr.onerror = function () {
        console.log("Request failed (network error)");
        callback(null);
    };

    xhr.send(JSON.stringify(data));

}

function getSyncData(token, callback) {

    var hostserver = getHostServer();
    if (!hostserver) return;

    // Make https request
    var xhr = new XMLHttpRequest();
    xhr.open("GET", hostserver + "/_matrix/client/v3/sync?timeout=30000");

    xhr.setRequestHeader("Authorization", `Bearer ${token}`);

    xhr.onload = function () {
        var responseText = JSON.parse(xhr.responseText);

        const rooms = responseText["rooms"] || {};
        const joinedRooms = rooms["join"] || {};

        for (var roomId in joinedRooms) {
            var roomData = joinedRooms[roomId];

            var name = '(no name)';
            var roomInfo = {'id': roomId};
            var names = {};

            var roomState = roomData["state"] || {};
            var roomEvents = roomState["events"] || [];

            for (var i = 0; i < roomEvents.length; i++) {
                var event = roomEvents[i];

                var content = event["content"] || {};
                var type = event["type"] || '';

                if (type === 'm.room.name') {
                    name = content["name"] || '(no name)';
                } else if (type === 'm.room.member') {
                    var sender = event["sender"] || '';
                    if (sender) {
                        var displayName = content["displayname"] || '(no displayname)';
                        names[sender] = displayName;
                    }
                }
            }

            roomInfo['name'] = name;
            roomInfo['names'] = names;

            var messages = {}
            var roomTimeline = roomData["timeline"] || {};
            var roomEvents = roomTimeline["events"] || [];

            for (var i = 0; i < roomEvents.length; i++) {
                var event = roomEvents[i];

                var content = event["content"] || {};
                var type = event["type"] || '';

                if (type === 'm.room.message') {
                    var messageData = {};

                    var timeMili = event["origin_server_ts"] || 1000;
                    var timeSec = timeMili / 1000;
                    var time = new Date(timeMili).toString();

                    var text = content["body"] || 'Error Getting Text';

                    var sender = event["sender"] || null;
                    if (sender) {
                        sender = names[sender] || sender;
                    }

                    messageData['time'] = time;
                    messageData['text'] = text;
                    messageData['sender'] = sender;

                    messages[timeMili] = messageData;
                }
            }

            roomInfo['messages'] = messages;
            syncData[name] = roomInfo;
        }

        callback(syncData)
    };

    xhr.onerror = function () {
        console.log("Request failed (network error)");
        callback(null);
    };

    xhr.send();
}

function matrixSendMessage(message) {
    var room = syncData[currentRoom];
    var id = room["id"];

    const txnId = Date.now().toString();

    hostserver = getHostServer();
    if (!hostserver) return;

    const url =
        hostserver +
        "/_matrix/client/v3/rooms/" +
        encodeURIComponent(id) +
        "/send/m.room.message/" +
        txnId;

    var xhr = new XMLHttpRequest();
    xhr.open("PUT", url, true);

    xhr.setRequestHeader("Authorization", `Bearer ${authToken}`);
    xhr.setRequestHeader("Content-Type", "application/json");

    const data = {
        msgtype: "m.text",
        body: message
    };

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            console.log("Status:", xhr.status);
            console.log("Response:", xhr.responseText);
        }
    };

    xhr.onerror = function () {
        console.log("Message failed to upload");
    };

    xhr.send(JSON.stringify(data));
}

// Send functions

function sendRooms(i) {
    var id = Object.keys(syncData)[i];

    Pebble.sendAppMessage(
        {'TYPE': 'ROOMS', 'ROOM_NAME': id},
        function() {
        },
        function(e) {
            console.log('Issue sending room: ', e);
        }
    );

    setTimeout( function() {
            sendRooms(i+1);
    }, 150);
}

function sendMessage(messages, i) {

    var ids = Object.keys(messages).sort();

    if (i >= ids.length) return;

    var id = ids[i];
    var message = messages[id];

    Pebble.sendAppMessage(
        {
            'TYPE': 'MESSAGE',
            'SENDER': message["sender"] || '(no sender)',
            'TEXT': message["text"] || '(no content)'
        },
        function() {
            console.log('Sending message ', message["text"] || '(no content)')
        },
        function(e) {
            console.log('Error sending message ', e);
        }
    );

    setTimeout( function() {
        sendMessage(messages, i+1);
    }, 150);

}

function sendMessages(room) {
    console.log('Checking messages for ', room);

    var roomData = syncData[room] || {};
    var messages = roomData["messages"] || {};

    sendMessage(messages, 0);
}


// Pebble Listeners

Pebble.addEventListener('ready', function(e) {
    console.log('PebbleKit JS ready!');

    if (!getSettings()) {
        console.log("Couldn't find conf");
        Pebble.sendAppMessage(
            {
                'TYPE': 'NOT_CONF'
            },
            function() {},
            function(e) {
                console.log('Issue sending configuration not found message to pebble ', e);
            }
        )
    } else {

        init();

    }
    
});

function init() {
    login(function(token) {
        if (token) {
            console.log('TOKEN:', token);
            getSyncData(token, function(data) {
                sendRooms(0);            
            });
        }
    });
}

Pebble.addEventListener('appmessage', function(e) {

    var type = e.payload.TYPE;

    if (type == 'ROOM_MESSAGES') {
        var room = e.payload.ROOM_NAME;
        currentRoom = room;
        sendMessages(room);
    } else if (type == 'SEND_MESSAGE') {
        var text = e.payload.TEXT;
        matrixSendMessage(text);
    }

});        

Pebble.addEventListener("showConfiguration", function() {
    console.log("Opening config page");
    Pebble.openURL("https://finbear2.github.io/bullet-time/src/pkjs/config.html");
});

Pebble.addEventListener("webviewclosed", function(e) {
    if (!e.response) return;

    try {
        var settings = JSON.parse(decodeURIComponent(e.response));
        localStorage.setItem("matrix_settings", JSON.stringify(settings));
        console.log("Settings saved", settings);
        init();
    } catch (err) {
        console.log("Failed to parse settings", err);
    }
});