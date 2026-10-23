document.body.appendChild(document.createElement("p")).textContent = "script.js ran; the style sheet made the text " + getComputedStyle(document.getElementById("styled")).color;
