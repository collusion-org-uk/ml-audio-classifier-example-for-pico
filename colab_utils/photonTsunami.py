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
  def save_wav_file(folder, fileNum, s):
    b = base64.b64decode(s.split(',')[1])
    
    file_name = "%s.wav" %(fileNum)
    #file_path = path.join(folder, datetime.now().strftime("%d-%m-%Y-%H-%M-%S.wav"))
    file_path = path.join(folder, file_name)
    print(f'Saving file: {file_path}')
    
    with open(file_path, 'wb') as f:
      f.write(b)

  output.register_callback('notebook.save_wav_file', save_wav_file)

  display.display(display.Javascript("""
  
    const recorderJsScript = document.createElement("script");
    const audioInputSelect = document.createElement("select");
    const loopButton = document.createElement("button");
    const recordButton = document.createElement("button");
	const LineStreamRecrderJsScript = document.createElement("script");
	LineStreamRecrderJsScript.src = "/nbextensions/google.colab/LineStreamTransformer.js";
    LineStreamRecrderJsScript.type = "text/javascript";
    document.body.append(LineStreamRecrderJsScript);
	let fileNum = 1391;
    let loopLock = false;
	
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
	  let readableStreamClosed = undefined;
      let keepReading = true;
      let term = undefined;
	  

	  // RH
	  
	  loopButton.innerHTML = "Play all in sequence";	
	  loopButton.id = "playAll";
	  document.querySelector("#output-area").appendChild(loopButton);
	  
	  
      const playTsumamiButton = document.createElement("button");	
      playTsumamiButton.innerHTML = "Play next Tsunami clip";	
	  playTsumamiButton.id = "play";
	  document.querySelector("#output-area").appendChild(playTsumamiButton);
	  
	  
	  loopButton.onclick = async () => {
        while (fileNum <2001){
		  if (!loopLock){
		     let element = document.getElementById('play');
		     element.click();
			 
		  }
		  await new Promise(resolve => setTimeout(resolve, 6000));
		}		
	  }
	  
	  
	  // End RH
	  
	  playTsumamiButton.onclick = async () => {
        loopLock=true;
	    const textEncoder = new TextEncoderStream();
        const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);

        const writer = textEncoder.writable.getWriter();

		term.write(message);
		let element = document.getElementById('record');
		element.click();
		fileNum++;
        await writer.write(fileNum);

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
			  await readableStreamClosed.catch(() => { /* Ignore the error */ });
            } catch (e) {}
          }

		  //await port.close();

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
    
        const decoder = new TextDecoderStream();
		let decodedMessage = "";
        while (port && keepReading) {
          try {
            //reader = port.readable.getReader();
			readableStreamClosed = port.readable.pipeTo(decoder.writable);
			reader = decoder.readable.pipeThrough(new TransformStream(new LineBreakTransformer())).getReader();
            //reader = port.readable.pipeThrough(new TransformStream(new LineBreakTransformer())).getReader();
            while (true) {
              const { value, done } = await reader.read();
              if (done) {
                keepReading = false;
				reader.releaseLock();
                break;
              }
			  //message = decoder.decode(value, { stream: true });
			  message = value;
			  //decodedMessage += message;
			  //let sDecodedMessage = String(decodedMessage);
			  //let n = decodedMessage.indexof("\\n",0);
			  //term.write(n);

			  if (message === "start"){
				term.write(message);
			  }
			  if (message === "end"){
				term.write(message);
				let element = document.getElementById('record');
				element.click();
			  }
			  
              //term.write(message);
            }
          } catch (error) {
            keepReading = false;
          } finally {
            await reader.releaseLock();
          }
        }
        
		//reader.cancel();
		await readableStreamClosed.catch(() => { /* Ignore the error */ });
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
	recordButton.id = "record";

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

          google.colab.kernel.invokeFunction('notebook.save_wav_file', ['###folder###', fileNum, text], {});
          
          recordButton.innerHTML = "⏺ Start Recording";
          recordButton.disabled = false;

          stream.getTracks().forEach(function(track) {
          if (track.readyState === 'live') {
              track.stop();
            }
          });
		  loopLock=false;
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
