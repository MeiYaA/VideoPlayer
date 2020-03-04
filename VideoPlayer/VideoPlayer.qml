import QtQuick 2.0
import QtQuick.Controls 2.5
import Moon 1.0

Rectangle {
    width: parent.width
    height: parent.height
    color: "black"

    property bool isPlaying: false
    property string videoPath: ""
    property bool isFirstPlay: true
    property bool volumeOff: false

    CVideoPlayer{
        id: cplayer
        anchors.fill: parent
    }

    MouseArea{
        anchors.fill: parent
        onClicked: {
            if(isPlaying)
            {
                isPlaying = false
                cplayer.pause();
            }
            else
            {
                isPlaying = true
                if(isFirstPlay)
                {
                    isFirstPlay = false
                    cplayer.startPlay(videoPath)
                    cplayer.visible = true
                }
                else
                    cplayer.play()
            }
        }
    }

    Connections{
        target: cplayer
        onSigShowTotalTime:
        {
            duation.text = totalTime
        }
    }

    Connections{
        target: cplayer
        onSigShowCurrentTime:
        {
            timing.text = currentTime
        }
    }

    Connections{
        target: cplayer
        onSigVideoIsOver:
        {
            cplayer.stop();
            isPlaying = false
            isFirstPlay = true;
            cplayer.visible = false;
        }
    }

    Connections{
        target: cplayer
        onSigSliderTotalValue:{
            progressBar.to = value
        }
        onSigSliderCurrentValue:{
            progressBar.value = currentvalue
        }
    }

    Rectangle{
        color: "black"
        width: parent.width
        height: parent.height * 1 / 18
        anchors.bottom: parent.bottom
        visible: true

        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered:{
                playerBottom.visible = true
            }
            onExited: {
                showBottonTimer.start()
            }
        }
    }

    Timer{
        id:showBottonTimer
        interval: 5000;
        running: true;
        repeat: true
        onTriggered:{
            playerBottom.visible = false
        }
    }

    Connections{
        target: progressBar
        onMoved:{
            cplayer.sliderMoved(progressBar.value)
        }
    }

    Rectangle{
        id: playerBottom
        color: "#DDDDDD"
        width: parent.width
        height: parent.height * 1 / 18
        anchors.bottom: parent.bottom
        visible: true

        Slider{
            id: progressBar
            width: parent.width
            height: parent.height * 2 / 5
            anchors.top: parent.top
            from: 0
            to: 100

            background: Rectangle {
                x: progressBar.leftPadding
                y: progressBar.topPadding + progressBar.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: progressBar.availableWidth
                height: implicitHeight
                radius: 2
                color: "#bdbebf"

                Rectangle {
                    width: progressBar.visualPosition * parent.width
                    height: parent.height
                    color: "#00BBFF"
                    radius: 2
                }
            }

            handle: Rectangle{
                x:progressBar.leftPadding + progressBar.visualPosition * (progressBar.availableWidth - width)
                y: progressBar.topPadding + progressBar.availableHeight / 2 - height / 2
                color:progressBar.pressed ? "#00BBFF":"white";
                border.color: "gray";
                width: 15;
                height: 15;
                radius: 12;
            }
        }

        Rectangle{
            id: controllerButtons
            width: parent.width
            height: parent.height * 3 / 5
            anchors.top: progressBar.bottom

            Rectangle{
                id: playOrPause
                width: parent.height
                height: parent.height
                anchors.left: parent.left
                Image {
                    width: parent.height
                    height: parent.height
                    anchors.fill: parent
                    source: isPlaying ? "./controller/pause.png" :
                                        "./controller/play.png"
                }
                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        if(isPlaying)
                        {
                            isPlaying = false
                            cplayer.pause();
                        }
                        else
                        {
                            isPlaying = true
                            if(isFirstPlay)
                            {
                                isFirstPlay = false
                                cplayer.startPlay(videoPath);
                                cplayer.visible = true
                            }
                            else
                               cplayer.play()
                        }
                    }
                }
            }

            Rectangle{
                id: stop
                width: parent.height
                height: parent.height
                anchors.left: playOrPause.right
                anchors.leftMargin: 20

                Image {
                    width: parent.height
                    height: parent.height
                    anchors.fill: parent
                    source: "./controller/stop.png"
                }
                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        isFirstPlay = true
                        isPlaying = false
                        progressBar.value = 0
                        duation.text = "00:00:00"
                        timing.text = "00:00:00"
                        cplayer.stop()
                        cplayer.visible = false
                    }
                }
            }
            Rectangle{
                id: nextVideo
                width: parent.height
                height: parent.height
                anchors.left: stop.right
                anchors.leftMargin: 20
                Image {
                    width: parent.height
                    height: parent.height
                    anchors.fill: parent
                    source: "./controller/next.png"
                }
            }
            Rectangle{
                id: playTime
                width: 4 * parent.height
                height: parent.height
                anchors.left: nextVideo.right
                anchors.leftMargin: 20

                Text {
                    id: timing
                    text: "00:00:00"
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: slash
                    text: " / "
                    anchors.left: timing.right
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: duation
                    text: "00:00:00"
                    anchors.left: slash.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle{
                id:fullScreen
                width: parent.height
                height: parent.height
                anchors.right: parent.right
                anchors.rightMargin: 10
                Image {
                    width: parent.height
                    height: parent.height
                    anchors.fill: parent
                    source: "./controller/fullScreen.png"
                }
            }

            Rectangle{
                id:star
                width: parent.height
                height: parent.height
                anchors.right: fullScreen.left
                anchors.rightMargin: 20
                Image {
                    width: parent.height
                    height: parent.height
                    anchors.fill: parent
                    source: "./controller/star.png"
                }
            }

            Rectangle{
                id:voiceController
                width: 4 * parent.height
                height: parent.height
                anchors.right: star.left
                anchors.rightMargin: 20

                Slider{
                    id:voiceBar
                    width: parent.width
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to:100
                    value: 50

                    background: Rectangle {
                        x: voiceBar.leftPadding
                        y: voiceBar.topPadding + voiceBar.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: voiceBar.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: "#bdbebf"

                        Rectangle {
                            width: voiceBar.visualPosition * parent.width
                            height: parent.height
                            color: "#00BBFF"
                            radius: 2
                        }
                    }

                    handle: Rectangle{
                        x:voiceBar.leftPadding + voiceBar.visualPosition * (voiceBar.availableWidth - width)
                        y: voiceBar.topPadding + voiceBar.availableHeight / 2 - height / 2
                        color:voiceBar.pressed ? "#00BBFF":"white";
                        border.color: "gray";
                        width: 10;
                        height: 10;
                        radius: 12;
                    }

                }

            }

            Connections{
                target: voiceBar
                onMoved:{
                    if(voiceBar.value === 0)
                        volumeOff = true
                    else
                        volumeOff = false
                    cplayer.voiceSliderMoved(voiceBar.value)
                }
            }

            Rectangle{
                id:voice
                width: parent.height
                height: parent.height

                anchors.right: voiceController.left
                Image {
                    width: voice.height
                    height: parent.height
                    anchors.fill: parent
                    source: volumeOff ? "./controller/volumeOff.png" : "./controller/volumeUp.png"
                }

                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        volumeOff = !volumeOff
                        if(volumeOff)
                        {
                            voiceBar.value = 0
                            cplayer.voiceSliderMoved(0)
                        }
                        else
                        {
                            voiceBar.value = 50
                            cplayer.voiceSliderMoved(50)
                        }
                    }
                }
            }
        }

    }
}
