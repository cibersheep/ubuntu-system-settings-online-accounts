import QtQuick 2.0
import Ubuntu.Components 1.3
import Ubuntu.Web 0.2

WebView {
    id: root

    property QtObject signonRequest

    onSignonRequestChanged: if (signonRequest) {
        signonRequest.authenticated.connect(onAuthenticated)
        url = signonRequest.startUrl
    }

    onLoadingStateChanged: {
        console.log("Loading changed")
        if (loading && !lastLoadFailed) {
            signonRequest.onLoadStarted()
        } else if (lastLoadSucceeded) {
            signonRequest.onLoadFinished(true)
        } else {
            signonRequest.onLoadFinished(false)
        }
    }
    onUrlChanged: signonRequest.currentUrl = url

    context: WebContext {
        dataPath: signonRequest ? signonRequest.rootDir : ""
    }

    function onAuthenticated() {
        /* Get the cookies and set them on the request */
        console.log("Authenticated; getting cookies")
        context.cookieManager.getCookiesResponse.connect(onGotCookies)
        context.cookieManager.getAllCookies()
        visible = false
    }

    function onGotCookies(requestId, cookies) {
        signonRequest.setCookies(cookies)
    }

    /* Taken from webbrowser-app */
    ProgressBar {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: units.dp(3)
        showProgressPercentage: false
        visible: root.loading
        value: root.loadProgress / 100
    }
}
