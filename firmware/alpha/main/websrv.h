/*
  VSCP droplet alpha webserver

  This file is part of the VSCP (https://www.vscp.org)

  The MIT License (MIT)
  Copyright © 2022-2025 Ake Hedman, the VSCP project <info@vscp.org>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  curl 192.168.43.130:80/hello
  curl -X POST --data-binary @registers.txt 192.168.1.112:80/echo > tmpfile
  curl -X PUT -d "0" 192.168.1.112:80/ctrl
  curl -X PUT -d "1" 192.168.1.112:80/ctrl

  1. "curl 192.168.43.130:80/hello"  - tests the GET "\hello" handler
  2. "curl -X POST --data-binary @anyfile 192.168.43.130:80/echo > tmpfile"
      * "anyfile" is the file being sent as request body and "tmpfile" is where the body of the response is saved
      * since the server echoes back the request body, the two files should be same, as can be confirmed using : "cmp
  anyfile tmpfile"
  3. "curl -X PUT -d "0" 192.168.43.130:80/ctrl" - disable /hello and /echo handlers
  4. "curl -X PUT -d "1" 192.168.43.130:80/ctrl" -  enable /hello and /echo handlers

*/

#ifndef __VSCP_ALPHA_WEBSRV_H__
#define __VSCP_ALPHA_WEBSRV_H__

#define WEBPAGE_PARAM_SIZE  128    // Max size of form parameters

// https://www.motobit.com/util/base64-decoder-encoder.asp
// <link href="data:image/x-icon;base64,YourBase64StringHere" rel="icon" type="image/x-icon" />
#define WEBPAGE_FAVICON "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" \
"tFxXFYVCPflIsfrglSq1++4lalmhUI/fW6AOvlKjfvGzH6pEYnpOvnkWZRo6yyrLaG5u5LnnnqGu" \
"ro57doQoLNbZ/36G8Uv+jMwEt2wNgSk41uNwvNehOapRGBa0NNkMnMlxsC1DQQhWl0BPd5ibW24P" \
"7pBXngWLjZ07d7LnxReob9jC+EWH9hPZ/Nr61Sbbm2yQ8PbRLCEUl5KK0gqDHTfYVBUJlCMZTlp0" \
"51qITy0n0fUGb+75NR1th+eeBVcbt3/1DoaH6ujq/AcXpl9FiH6Uktxzm01tlSCT8ek4mWNLVGeJ" \
"Ldi52cYOKeJJyXfu2kTNtoe56aYbOXXwZTbUWGRzI/z3wF7qGjZQXl72xV5GtSvWsKz6l7xW/wBt" \
"bW3EYjEKywdoPZoknkhyYQp2Xz/FqFNBUdVmjgw0MJA+wdbmpdzxyCOUlETo7zrEoU/aSWZ9WNaY" \
"t2VxLW9D3/eJxxPE43FSqWli7Ue50LYHo7KR+x9/GiUlb770WwqccbzIKnY9/CRLl5bQ3n4U3/PZ" \
"tHkz0Wj02gF8friex/6Xfkdp+jPGLmewN9xLedUK+g78gRsbKnn72Dlqb32Ub337wQX///LP2QWG" \
"UsGdyfM8BgYHWd+4CVGzlb8fGWKUKtauW7/ov/93BQA6O2Ic2v83Riem+fpDj7G9pYWpxDSfftpL" \
"WWkZq1bVLXop+R9VbeGtj3wRLQAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAxNy0wOC0yOVQxMDoxMDo1" \
"OSswMDowMJ6/lxIAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMTctMDgtMjlUMTA6MTA6NTkrMDA6MDDv" \
"4i+uAAAARnRFWHRzb2Z0d2FyZQBJbWFnZU1hZ2ljayA2LjcuOC05IDIwMTQtMDUtMTIgUTE2IGh0" \
"dHA6Ly93d3cuaW1hZ2VtYWdpY2sub3Jn3IbtAAAAABh0RVh0VGh1bWI6OkRvY3VtZW50OjpQYWdl" \
"cwAxp/+7LwAAABh0RVh0VGh1bWI6OkltYWdlOjpoZWlnaHQAMTkyDwByhQAAABd0RVh0VGh1bWI6" \
"OkltYWdlOjpXaWR0aAAxOTLTrCEIAAAAGXRFWHRUaHVtYjo6TWltZXR5cGUAaW1hZ2UvcG5nP7JW" \
"TgAAABd0RVh0VGh1bWI6Ok1UaW1lADE1MDQwMDE0NTl00+TtAAAAD3RFWHRUaHVtYjo6U2l6ZQAw" \
"QkKUoj7sAAAAVnRFWHRUaHVtYjo6VVJJAGZpbGU6Ly8vbW50bG9nL2Zhdmljb25zLzIwMTctMDgt" \
"MjkvMDNjODFlNDg1NzQ5OTkwNTJlNmI1M2ZhZjU4ZWUzNTUuaWNvLnBuZ4SbW3QAAAAASUVORK5C" \
"YII="

// https://codebeautify.org/css-beautify-minify
#define WEBPAGE_STYLE_CSS "div,fieldset,input,select{padding: 5px;font-size: 1.0em}fieldset{background: #4b4b4e}p{margin: 0.5em 0}input{width: 100%%;box-sizing: border-box;-webkit-box-sizing: border-box;-moz-box-sizing: border-box;background: #dddddd;color: #000000}input[type=checkbox],input[type=radio]{width: 1em;margin-right: 6px;vertical-align: -1px}input[type=range]{width: 99%%}select{width: 100%%;background: #dddddd;color: #000000}textarea{resize: vertical;width: 98%%;height: 318px;padding: 5px;overflow: auto;background: #e9e6e6;color: #65c115b6}body{text-align: center;font-family: verdana, sans-serif;background: #252525}button{border: 1;border-radius: 0.5rem;background: #d3d3d0;color: #000000;line-height: 2.4rem;font-size: 1.2rem;width: 100%%;-webkit-transition-duration: 0.7s;transition-duration: 0.7s;cursor: pointer}button:hover{background: #375733}.bred{background: #d43535}.bred:hover{background: #931f1f}.bgrn{background: #47c266}.bgrn:hover{background: #296939}.byell{background: #f0ee81}.byell:hover{background: #68642e}a{color: #1fa3ec;text-decoration: none}.p{float: left;text-align: left}.q{float: right;text-align: right}.r{border-radius: 0.3em;padding: 2px;margin: 6px 2px}.hf{display: none}td{padding-left: 30px;padding-right: 15px;padding-bottom: 10px}.name{font-family: Arial, Helvetica, sans-serif;font-size: small;font-weight: bold;color: #ffffff}.prop{font-family: Arial, Helvetica, sans-serif;font-size: small;font-weight: lighter;color: #a7aca7}.infoheader{font-family: Arial, Helvetica, sans-serif;font-size: normal;font-weight: lighter;color: #ede02c}"

/*
// https://github.com/Jeija/esp32-softap-ota/blob/master/main/web/index.html
function startUpload() {
				var otafile = document.getElementById("otafile").files;

				if (otafile.length == 0) {
					alert("No file selected!");
				} else {
					document.getElementById("otafile").disabled = true;
					document.getElementById("upload").disabled = true;

					var file = otafile[0];
					var xhr = new XMLHttpRequest();
					xhr.onreadystatechange = function() {
						if (xhr.readyState == 4) {
							if (xhr.status == 200) {
								document.open();
								document.write(xhr.responseText);
								document.close();
							} else if (xhr.status == 0) {
								alert("Server closed the connection abruptly!");
								location.reload()
							} else {
								alert(xhr.status + " Error!\n" + xhr.responseText);
								location.reload()
							}
						}
					};

					xhr.upload.onprogress = function (e) {
						var progress = document.getElementById("progress");
						progress.textContent = "Progress: " + (e.loaded / e.total * 100).toFixed(0) + "%";
					};
					xhr.open("POST", "/update", true);
					xhr.send(file);
				}
			}
*/

/*
#define TTT "function startUpload() { " \
"	var otafile = document.getElementById('otafile').files; " \
" " \
"	if (otafile.length == 0) { " \
"		alert('No file selected!'); " \
"	} else {" \
"		document.getElementById('otafile').disabled = true; " \
"		document.getElementById('upload').disabled = true; " \
" " \
"		var file = otafile[0]; " \
"		var xhr = new XMLHttpRequest(); " \
" " \
"		xhr.onreadystatechange = function() { " \
"			if (xhr.readyState == 4) { " \
"				if (xhr.status == 200) { " \
"					document.open(); " \
"					document.write(xhr.responseText); " \
"					document.close(); " \
"				} else if (xhr.status == 0) { " \
"					alert('Server closed the connection abruptly!'); " \
"					location.reload() " \
"				} else { " \
"					alert(xhr.status + ' Error!' + xhr.responseText); " \
"					location.reload() " \
"				} " \
"			} " \
"		}; " \
" " \
"		xhr.upload.onprogress = function (e) { " \
"			var progress = document.getElementById('progress'); " \
"			progress.textContent = 'Progress: ' + (e.loaded / e.total * 100).toFixed(0) + '%%'; " \
"		}; " \
" } " \
" alert(\"hello\");" \
"}"
*/


#define WEBPAGE_JS1 "function startUpload(){var e,t=document.getElementById(\"otafile\").files;0==t.length?alert(\"No file selected!\"):(document.getElementById(\"otafile\").disabled=!0,document.getElementById(\"upload\").disabled=!0,t=t[0],(e=new XMLHttpRequest).onreadystatechange=function(){4==e.readyState&&(200==e.status?(document.open(),document.write(e.responseText),document.close()):(0==e.status?alert(\"Server closed the connection abruptly!\"):alert(e.status+\" Error!\"+e.responseText),location.reload()))},e.upload.onprogress=function(e){document.getElementById(\"progress\").textContent=\"Progress: \"+(e.loaded/e.total*100).toFixed(0)+\"%%\"},e.open(\"POST\",\"/upgrdlocal\",!0),e.send(t))}"
#define WEBPAGE_JS2 "function startUploadSibLocal(){var e,t=document.getElementById(\"otafile_sib\").files;0==t.length?alert(\"No file selected!\"):(document.getElementById(\"otafile_sib\").disabled=!0,document.getElementById(\"upload_sib\").disabled=!0,t=t[0],(e=new XMLHttpRequest).onreadystatechange=function(){4==e.readyState&&(200==e.status?(document.open(),document.write(e.responseText),document.close()):(0==e.status?alert(\"Server closed the connection abruptly!\"):alert(e.status+\" Error!\"+e.responseText),location.reload()))},e.upload.onprogress=function(e){document.getElementById(\"progress_sib\").textContent=\"Progress: \"+(e.loaded/e.total*100).toFixed(0)+\"%%\"},e.open(\"POST\",\"/upgrdSiblingLocal\",!0),e.send(t))}"

/*>>
  Page start HTML
  Parameter 1: Page head
  Parameter 2: Section header  
  "<link rel=\"stylesheet\" href=\"style.css\" /></head><body><div " \
  "<link rel=\"icon\" href=\"favicon.ico\">" \
*/
#define WEBPAGE_START_TEMPLATE "<!DOCTYPE html><html lang=\"en\" class=\"\"><head><meta charset='utf-8'>" \
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\" />" \
"<title>Droplet Alpha node - Main Menu</title>"\
"<script>" \
WEBPAGE_JS1 \
WEBPAGE_JS2 \
"</script>" \
"<style>" \
WEBPAGE_STYLE_CSS \
"</style>" \
"<link href=\"data:image/x-icon;base64," \
WEBPAGE_FAVICON \
"\" rel=\"icon\" type=\"image/x-icon\" />" \
"</head><body><div " \
"style='text-align:left;display:inline-block;color:#eaeaea;min-width:340px;max-width:600px;'>" \
"<div style='text-align:center;color:#eaeaea;'>" \
"<h3>%s</h3></div>" \
"<div style='text-align:center;color:#f7f1a6;'><h4>%s</h4></div>"

/*>>
  Page end HTML
  Parameter 1: Page head
  Parameter 2: Section header
*/
#define WEBPAGE_END_TEMPLATE "<div style='text-align:right;font-size:11px;'><hr /><form id=but14 style=\"display: block;\" "\
"action='index.html' method='get'><button class=\"byell\">Main Menu</button></form>"\
"<hr /><div style='text-align:right;font-size:11px;'>%s - <a href='https://vscp.org' target='_blank' "\
"style='color:#aaa;'>%s -- vscp.org</a></div>"\
"</div></body></html>"

#define WEBPAGE_END_TEMPLATE_NO_RETURN "<div style='text-align:right;font-size:11px;'>"\
"<hr /><div style='text-align:right;font-size:11px;'>%s - <a href='https://vscp.org' target='_blank' "\
"style='color:#aaa;'>%s -- vscp.org</a></div>"\
"</div></body></html>"

#define CONFIG_DEFAULT_BASIC_AUTH_USERNAME "vscp"
#define CONFIG_DEFAULT_BASIC_AUTH_PASSWORD "secret"

#define HTTPD_401 "401 UNAUTHORIZED" /*!< HTTP Response 401 */

typedef struct {
  char *username;
  char *password;
} basic_auth_info_t;

/*!
  Start the webserver
  @return esp error code
*/

httpd_handle_t
start_webserver(void);

/*!
  Stop the webserver
  @param server Server handle
  @return esp error code
*/

esp_err_t
stop_webserver(httpd_handle_t server);

#endif