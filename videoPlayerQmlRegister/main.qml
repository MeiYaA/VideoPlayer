import QtQuick 2.12
import QtQuick.Window 2.12
import Star 1.0

Window {
    visible: true
    width: 1075
    height: 670
    title: qsTr("Hello World")

    Rectangle{
        width: parent.width
        height: parent.height
        VidePlayer{
            anchors.fill: parent
            width: parent.width
            height: parent.height
            vwidth: parent.width
            vheight: parent.height
        }
    }
}
