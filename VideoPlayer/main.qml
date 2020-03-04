import QtQuick 2.12
import QtQuick.Window 2.12

Window {
    visible: true
    width: 640
    height: 480
    title: qsTr("Hello World")

    VideoPlayer{
        id: myPlayer;
        anchors.fill: parent
        videoPath: "rtsp://192.168.1.13:8554/test.ts"
    }
}
