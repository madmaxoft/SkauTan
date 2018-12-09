var g_MaxSongIndex = -1





/** Appends a new div element containing only text to the specified parent. */
function appendTextDiv(a_Parent, a_Class, a_Text)
{
	if ((a_Text === undefined) || (a_Text === null))
	{
		console.log("Attempting to add an empty text div", a_Class, " to ", a_Parent);
		return;
	}
	var div = document.createElement("div");
	div.setAttribute("class", a_Class);
	div.appendChild(document.createTextNode(a_Text));
	a_Parent.appendChild(div);
}





/** Appends a new div containing the voting buttons for the specified song hash in the specified category,
to the specified parent. */
function appendVoteButtons(a_Parent, a_SongHash, a_RatingCategory, a_Class)
{
	var div = document.createElement("div");
	div.setAttribute("class", "voteContainer" + a_Class);
	var i;
	for (i = 5; i >= 1; --i)
	{
		var img = document.createElement("img");
		img.setAttribute("src", "static/voteButton" + i + ".png")
		img.setAttribute("class", "voteButton")
		img.setAttribute("OnClick", "vote(this, \"" + a_SongHash + "\", \"" + a_RatingCategory + "\", " + i + ")");
		div.appendChild(img);
	}
	a_Parent.appendChild(div);
}





/** Adds the specified song description into the UI.
Only adds the UI if the song index is larger than currently remembered g_MaxSongIndex.
Also updates the global g_MaxSongIndex if the song's index is larger.
a_Desc is the song's description:
{
	hash: "",
	fileName: "",
	author: "",
	title: "",
	genre: "",
}
*/
function addSong(a_Desc)
{
	if (a_Desc === null)
	{
		return;
	}
	if (a_Desc.index * 1 <= g_MaxSongIndex * 1)
	{
		return;
	}
	// console.log("Adding song ", a_Desc);
	var fragment = document.createDocumentFragment();
	var container = document.createElement("div");
	container.setAttribute("class", "songContainer");
	var cInfo = document.createElement("div");
	cInfo.setAttribute("class", "songInfoContainer");
	container.appendChild(cInfo);
	var authorTitle = a_Desc.author;
	if (authorTitle !== "")
	{
		authorTitle += " - ";
	}
	authorTitle += a_Desc.title;
	appendTextDiv(cInfo, "at",           authorTitle);
	appendTextDiv(cInfo, "fileName",     a_Desc.fileName);
	appendTextDiv(cInfo, "genre",        a_Desc.genre);
	if (a_Desc.mpm > 0)
	{
		var mpm = Math.floor(a_Desc.mpm * 10) / 10;
		appendTextDiv(cInfo, "mpm", mpm + " MPM | ");
	}

	appendTextDiv(container, "voteLabelRC",  gText["Rhythm clarity:"]);
	appendTextDiv(container, "voteLabelGT",  gText["Genre typicality:"]);
	appendTextDiv(container, "voteLabelPop", gText["Popularity:"]);
	appendVoteButtons(container, a_Desc.hash, "rhythmClarity",   "RC");
	appendVoteButtons(container, a_Desc.hash, "genreTypicality", "GT");
	appendVoteButtons(container, a_Desc.hash, "popularity",      "Pop");

	fragment.appendChild(container);
	var songs = document.getElementById("songs");
	songs.insertBefore(fragment, songs.firstChild);
	g_MaxSongIndex = a_Desc.index * 1;
}




/** Sends a vote to the server.
a_Button is the reference to the button being pressed.
a_VoteType is the type of the voting parameter ("rhythmClarity", "genreTypicality" or "popularity")
a_VoteValue is the actual vote to be sent (1 - 5). */
function vote(a_Button, a_SongHash, a_VoteType, a_VoteValue)
{
	// Highlight and disable the button for a while:
	a_Button.style.opacity = '0.5';
	a_Button.enabled = false
	setTimeout(function()
	{
		a_Button.style.opacity = '1';
		a_Button.enabled = false;
	}, 1000);

	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function()
	{
		if ((this.readyState === 4) && (this.status === 200))
		{
			// TODO: Update the relevant song's rating
			// document.getElementById("demo").innerHTML = this.responseText;
		}
	};
	xhttp.open("POST", "api/vote", true);
	xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
	xhttp.send("songHash=" + a_SongHash + "&voteType=" + a_VoteType + "&voteValue=" + a_VoteValue);
}




/** Sends a "playlist" API query to the server. */
function pollPlaylist()
{
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function()
	{
		if ((this.readyState === 4) && (this.status === 200))
		{
			var resp
			try
			{
				resp = JSON.parse(this.responseText);
			}
			catch (err)
			{
				console.log("Parsing failed: ", err);
			}
			// console.log("Received response: ", resp);
			if (resp instanceof Array)
			{
				resp.forEach(addSong);
			}
		}
	};
	xhttp.open("POST", "api/playlist", true);
	xhttp.setRequestHeader("x-SkauTan-playlist-start", g_MaxSongIndex * 1 + 1);
	xhttp.send();
}





// Fill the playlist with initial data and start polling the server for more playlist entries:
setInterval(pollPlaylist, 1000);
