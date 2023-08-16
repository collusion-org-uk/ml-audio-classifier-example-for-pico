#
# Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

from IPython import display 
from google.colab import output
import base64
from datetime import datetime
from os import path

def record_wav_file(folder):
  def save_wav_file(folder, s):
    b = base64.b64decode(s.split(',')[1])
    
    file_path = path.join(folder, datetime.now().strftime("%d-%m-%Y-%H-%M-%S.wav"))
    
    print(f'Saving file: {file_path}')
    
    with open(file_path, 'wb') as f:
      f.write(b)

  output.register_callback('notebook.save_wav_file', save_wav_file)

  display.display(display.Javascript("""
  
    const recorderJsScript = document.createElement("script");
    const audioInputSelect = document.createElement("select");
    const recordButton = document.createElement("button");
	
  if ('serial' in navigator) {
      const scriptElement = document.createElement("script");
      scriptElement.src = "https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.5/xterm.min.js";
      document.body.appendChild(scriptElement);

      const linkElement = document.createElement("link");
      linkElement.rel = "stylesheet"
      linkElement.href = "https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.5/xterm.min.css";
      document.body.appendChild(linkElement);

      const connectDisconnectButton = document.createElement("button");
	  


      connectDisconnectButton.innerHTML = "Connect Port";

      document.querySelector("#output-area").appendChild(connectDisconnectButton);

      terminalDiv = document.createElement("div");
      terminalDiv.style = "margin: 5px";

      document.querySelector("#output-area").appendChild(terminalDiv);

      let port = undefined;
      let reader = undefined;
      let keepReading = true;
      let term = undefined;
	  
	  
	  // RH
	  
      const playTsumamiButton = document.createElement("button");	
      playTsumamiButton.innerHTML = "Play next Tsunami clip";	
	  document.querySelector("#output-area").appendChild(playTsumamiButton);
	  // End RH
	  
	  playTsumamiButton.onclick = async () => {
	    const textEncoder = new TextEncoderStream();
        const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);

        const writer = textEncoder.writable.getWriter();

        await writer.write("1");
		
		writer.close();
		await writableStreamClosed;
		await writer.releaseLock();
	  }

      connectDisconnectButton.onclick = async () => {
        if (port !== undefined) {
          if (reader !== undefined) {
            keepReading = false;
            try {
              await reader.cancel();
            } catch (e) {}
          }
          port = undefined;
          reader = undefined;

          connectDisconnectButton.innerHTML = "Connect Port";

          return;
        }

        port = await navigator.serial.requestPort();
        keepReading = true;

        connectDisconnectButton.innerHTML = "Disconnect Port";
        
        await port.open({ baudRate: 115200 });

        if (term === undefined) {
          term = new Terminal();
          term.open(terminalDiv);
        }
        term.clear();
    
        const decoder = new TextDecoder();
		let decodedMessage = "";
        while (port && keepReading) {
          try {
            reader = port.readable.getReader();
          
            while (true) {
              const { value, done } = await reader.read();
              if (done) {
                keepReading = false;
                break;
              }
			  message = decoder.decode(value, { stream: true });
			  decodedMessage += message;
			  

			  if (message === "start"){
				term.write("yes - start");
			  }
			  if (message === "end"){
				term.write("yes - end");
			  }
			  
              term.write(message);
            }
          } catch (error) {
            keepReading = false;
          } finally {
            await reader.releaseLock();
          }
        }
        
        await port.close();

        port = undefined;
        reader = undefined;

        connectDisconnectButton.innerHTML = "Connect Port";
      };
    } else {
      document.querySelector("#output-area").appendChild(document.createTextNode(
        "Oh no! Your browser does not support Web Serial!"
      ));
    }
  """+
  """


    recorderJsScript.src = "https://sandeepmistry.github.io/Recorderjs/dist/recorder.js";
    recorderJsScript.type = "text/javascript";

    recordButton.innerHTML = "⏺ Start Recording";

    document.body.append(recorderJsScript);
    document.querySelector("#output-area").appendChild(audioInputSelect);
    document.querySelector("#output-area").appendChild(recordButton);

    async function updateAudioInputSelect() {
      while (audioInputSelect.firstChild) {
        audioInputSelect.removeChild(audioInputSelect.firstChild);
      }

      const deviceInfos = await navigator.mediaDevices.enumerateDevices();

      for (let i = 0; i !== deviceInfos.length; ++i) {
        const deviceInfo = deviceInfos[i];
        const option = document.createElement("option");
        
        option.value = deviceInfo.deviceId;
        
        if (deviceInfo.kind === "audioinput") {
          option.text = deviceInfo.label || "Microphone " + (audioInputSelect.length + 1);
          option.selected = (option.text.indexOf("MicNode") !== -1);
          audioInputSelect.appendChild(option);
        }
      }
    }

    const blob2text = (blob) => new Promise(resolve => {
      const reader = new FileReader();
      reader.onloadend = e => resolve(e.srcElement.result);
      reader.readAsDataURL(blob)
    });

    let recorder = undefined;
    let stream = undefined;

    // https://www.html5rocks.com/en/tutorials/getusermedia/intro/
    recordButton.onclick = async () => {
      if (recordButton.innerHTML != "⏺ Start Recording") {
        recordButton.disabled = true;
        recorder.stop();

        recorder.exportWAV(async (blob) => {
          const text = await blob2text(blob);

          google.colab.kernel.invokeFunction('notebook.save_wav_file', ['###folder###', text], {});
          
          recordButton.innerHTML = "⏺ Start Recording";
          recordButton.disabled = false;

          stream.getTracks().forEach(function(track) {
          if (track.readyState === 'live') {
              track.stop();
            }
          });
        });

        return;
      }

      const constraints = {
        audio: {
          deviceId: {
          },
          sampleRate: 16000
        },
        video: false
      };

      stream = await navigator.mediaDevices.getUserMedia(constraints);

      if (audioInputSelect.value === "") {
        await updateAudioInputSelect();

        stream.getTracks().forEach(function(track) {
          if (track.readyState === 'live') {
            track.stop();
          }
        });

        constraints.audio.deviceId.exact = audioInputSelect.value;
        stream = await navigator.mediaDevices.getUserMedia(constraints);
      }

      const audioContext = new AudioContext({
        sampleRate: 16000
      });
      
      const input = audioContext.createMediaStreamSource(stream);

      recorder = new Recorder(input, {
         numChannels: 1
      });

      recordButton.innerHTML = "⏹ Stop Recording";

      recorder.record();
    };

    updateAudioInputSelect();
  """.replace('###folder###', folder)))
