/*
 * Copyright (C) 2020 Richard Yu <yurichard3839@gmail.com>
 *
 * This file is part of ClashXW.
 *
 * ClashXW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ClashXW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with ClashXW.  If not, see <https://www.gnu.org/licenses/>.
 */

// This file contains code from
// https://github.com/marcuswestin/WebViewJavascriptBridge/blob/master/WebViewJavascriptBridge/WebViewJavascriptBridge_JS.m
// which is licensed under the MIT License.
// See licenses/WebViewJavascriptBridge for more information.

(function() {
	if (window.WebViewJavascriptBridge) {
		return;
	}

	var messageHandlers = {};
	var responseCallbacks = {};
	var uniqueId = 0;

	function _doSend(message, responseCallback) {
		if (responseCallback) {
			var callbackId = 'cb_' + (++uniqueId) + '_' + (+new Date);
			responseCallbacks[callbackId] = responseCallback;
			message['callbackId'] = callbackId;
		}
		window.chrome.webview.postMessage(message);
	}

	window.chrome.webview.addEventListener('message', function(ev) {
		var message = ev.data;
		var responseCallback;

		if (message.responseId) {
			responseCallback = responseCallbacks[message.responseId];
			if (!responseCallback) {
				return;
			}
			responseCallback(message.responseData);
			delete responseCallbacks[message.responseId];
		} else {
			if (message.callbackId) {
				var callbackResponseId = message.callbackId;
				responseCallback = function(responseData) {
					_doSend({ handlerName: message.handlerName, responseId: callbackResponseId, responseData: responseData });
				};
			}

			var handler = messageHandlers[message.handlerName];
			if (!handler) {
				console.log("WebViewJavascriptBridge: WARNING: no handler for message from native:", message);
			} else {
				handler(message.data, responseCallback);
			}
		}
	});

	window.WebViewJavascriptBridge = {
		registerHandler: function(handlerName, handler) {
			messageHandlers[handlerName] = handler;
		},
		callHandler: function(handlerName, data, responseCallback) {
			if (arguments.length == 2 && typeof data == 'function') {
				responseCallback = data;
				data = null;
			}
			_doSend({ handlerName: handlerName, data: data }, responseCallback);
		},
		disableJavscriptAlertBoxSafetyTimeout: function() {},
	};

	setTimeout(function() {
		var callbacks = window.WVJBCallbacks;
		if (callbacks) {
			delete window.WVJBCallbacks;
			for (var i = 0; i < callbacks.length; ++i) {
				callbacks[i](WebViewJavascriptBridge);
			}
		}
	}, 0);
})();
