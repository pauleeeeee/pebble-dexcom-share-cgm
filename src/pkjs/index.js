// require clay package for settings
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
// override the handleEvents feature in Clay. This way we can intercept values here in JS and then figure out what to do with them
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var dexcom = {
    applicationID: "d8665ade-9673-4e27-9ff6-92db4ce13d13",
    authURL: "https://share2.dexcom.com/ShareWebServices/Services/General/LoginPublisherAccountByName",
    dataURL: "https://share2.dexcom.com/ShareWebServices/Services/Publisher/ReadPublisherLatestGlucoseValues",
    accept: "application/json",
    contentType: "application/json",
    agent: "Dexcom Share/3.0.2.11 CFNetwork/672.0.2 Darwin/14.0.0"
}

var accessCredentials = {
    sessionID: "",
    username: "",
    password: "",
    lastRefreshed: 0,
    expiresIn: 86400 //1 day in seconds
}

// //declare initial values or lackthereof
var CGMdata;
//holding objects for settings
var storedSettings = null;
//last alert record
lastAlert = {
    low: new Date("1-1-1990").getTime(),
    high: new Date("1-1-1990").getTime()
};

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});


//this function is called when the watchface is ready
Pebble.addEventListener("ready", function(e) {
    console.log('ready');
    if (localStorage.getItem("accessCredentials")) {
        accessCredentials = JSON.parse(localStorage.getItem("accessCredentials"));
    }

    var CGM = JSON.parse(localStorage.getItem("CGMdata"));
    if (CGM){CGMdata = CGM} else {CGMdata={egvs:[{systemTime:""}]}};

    lAlert = JSON.parse(localStorage.getItem("lastAlerts"));
    if(lAlert){
        lastAlert = lAlert;
    }
    //get settings from local storage
    var settings = JSON.parse(localStorage.getItem("storedSettings"));

    //if there are settings assign them and begin the fetch loop
    if (settings) {
        // console.log('found settings');
        storedSettings = settings;
        //start fetchLoop
        fetchLoop();

    } else {
        // console.log('no settings');
        //show notification stating that settings must be configured. Should only ever happen once.
        Pebble.showSimpleNotificationOnPebble("Configuration needed","Go to the watchface settings inside the Pebble phone app to configure this watchface.")
    }

});


//when the config page is closed...
Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) { return; }

    //get settings from response object
    storedSettings = clay.getSettings(e.response, false);
    accessCredentials.username = storedSettings.UserName.value;
    accessCredentials.password = storedSettings.Password.value;
    //save the settings directly to local storage.
    localStorage.setItem("storedSettings", JSON.stringify(storedSettings));
    localStorage.setItem("accessCredentials", JSON.stringify(accessCredentials));
    console.log(JSON.stringify(storedSettings));
    console.log(JSON.stringify(accessCredentials));

    //start refresh loop
    console.log('refresh loop');
    accessCredentials.sessionID = "";
    fetchLoop();
});

function fetchLoop(){
    if (((new Date().getTime() - accessCredentials.lastRefreshed) / 1000) > accessCredentials.expires_in || accessCredentials.sessionID == "" || accessCredentials.sessionID == "00000000-0000-0000-0000-000000000000") {
        //refresh the token then fetch data
        console.log('refresh the token');
        refreshDexcomAccessToken();
    } else {
        //fetch data
        console.log('get the data');
        getDexcomData();
    }

    //loop every 60 seconds
    setTimeout(
        function handler() {
            fetchLoop();
        },
        60000
    );
}

function refreshDexcomAccessToken(){
    var data = {
        "password": accessCredentials.password,
        "applicationId": dexcom.applicationID,
        "accountName": accessCredentials.username
    };
    var xhr = new XMLHttpRequest();
    //xhr.withCredentials = true;
    xhr.open("POST", dexcom.authURL);
    xhr.setRequestHeader("User-Agent", dexcom.agent);
    xhr.setRequestHeader("Content-type", dexcom.contentType);
    xhr.setRequestHeader('Accept', dexcom.accept);

    xhr.onload = function(e) {
        if (xhr.readyState == 4) {
          // 200 - HTTP OK
            if(xhr.status == 200) {
                var response = xhr.responseText;
                console.log(response);
                var formattedResponse = xhr.responseText.replace(/['"]+/g, '');
                console.log(formattedResponse);
                accessCredentials.sessionID = formattedResponse;
                accessCredentials.lastRefreshed = new Date().getTime();
                localStorage.setItem("accessCredentials",JSON.stringify(accessCredentials));
                getDexcomData();
            } else if(xhr.status == 400){
                if(accessCredentials.sessionID != ""){Pebble.showSimpleNotificationOnPebble("Authorization needed","Your log in to Dexcom servers has expired. Please visit the watch configuration page to log in again.");};
                accessCredentials.sessionID = "";
                console.log("failed to get data - need to re authenticate");
            }
        }
    }

    xhr.send(JSON.stringify(data));

}

function getDexcomData(){
    var xhr = new XMLHttpRequest();
    //xhr.withCredentials = true;
    var URL = dexcom.dataURL + '?sessionID=' + accessCredentials.sessionID + '&minutes=' + 1440 + '&maxCount=' + 1;
    
    xhr.open("POST", URL);

    xhr.setRequestHeader("User-Agent", dexcom.agent);
    xhr.setRequestHeader("Content-type", dexcom.contentType);
    xhr.setRequestHeader('Accept', dexcom.accept);
    xhr.setRequestHeader('Content-Length', 0);
    xhr.onload = function(e) {
        if (xhr.readyState == 4) {
          // 200 - HTTP OK
            if(xhr.status == 200) {
                // console.log(xhr.responseText);
                // var response = JSON.parse(xhr.responseText);
                // console.log(response);
                var response = JSON.parse(xhr.responseText);
                console.log(xhr.responseText);
                //check to see if this data is old. If it's old... do nothing.
                if (JSON.stringify(response) == JSON.stringify(CGMdata)) {
                    console.log('did not send appmessage');
                    return
                } else {
                    //if it's new, construct an appmessage using the appropriate keys defined in package.json and in nightscout-double-vision.h
                    //minutesAgo could be improved   
                    var message = {
                        "SGV": response[0].Value+"",
                        "Direction": Number(response[0].Trend),
                        "MinutesAgo": 0,
                        "SendAlert": makeAlert(response[0].Value),
                        "RespectQuietTime": (storedSettings.RespectQuietTime.value ? 1: 0)
                    };

                    console.log(JSON.stringify(message));

                    //send the app message and use a success callback. On success, store the message to localStorage so that it can be compared on the next loop.
                    Pebble.sendAppMessage(message,function(){
                        CGMdata = response;
                        localStorage.setItem("CGMdata", JSON.stringify(response));
                        console.log('message sent!');
                    });
                }
            } else if (xhr.status == 401){
                console.log("failed to get data - need to refresh access token");
                var response = xhr.responseText;
                console.log(response);
                refreshDexcomAccessToken();
            } else {
                console.log('other')
                console.log(xhr.status);
                console.log(xhr.responseText);
                console.log(xhr.responseType);
                refreshDexcomAccessToken();
            }
        }
    }

    xhr.send();
    
}


// //takes the string returned by nightscout and turns it into an int
// function directionStringToInt(direction) {
//     switch (direction) {
//         case "none":
//             return 0;
//         case "doubleUp":
//             return 1;
//         case "singleUp":
//             return 2;
//         case "fortyFiveUp":
//             return 3;
//         case "flat":
//             return 4;
//         case "fortyFiveDown":
//             return 5;
//         case "singleDown":
//             return 6;
//         case "doubleDown":
//             return 7;
//         case 'notComputable':
//             return 8;
//         case 'rateOutOfRange':
//             return 9;
//         default:
//             return 0;
//     }
// };



function makeAlert(sgv){

    if (!storedSettings.EnableAlerts.value) {
        // console.log ('alerts disabled');
        return 0;

    } else {

        var integer = 0;
    
        if (sgv < storedSettings.LowAlertValue.value && (new Date().getTime() - lastAlert.low) > storedSettings.LowSnoozeTimeout.value * 60 * 1000){
            integer = Number(storedSettings.LowVibePattern.value);
            lastAlert.low = new Date().getTime();
            // console.log('met low criteria');

        } else if (sgv > storedSettings.HighAlertValue.value && (new Date().getTime() - lastAlert.high) > storedSettings.HighSnoozeTimeout.value * 60 * 1000){
            integer = Number(storedSettings.HighVibePattern.value);
            lastAlert.high = new Date().getTime();
            // console.log('met high criteria');s
        }
        // console.log(JSON.stringify(lastAlert));
        localStorage.setItem("lastAlert", JSON.stringify(lastAlert));
    
        return integer;

    }

}


