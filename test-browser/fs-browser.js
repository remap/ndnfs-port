// TODO: 
// 1. Now the client cannot tell the difference between empty directory and request timeout, need to modify server.
// 2. ndnfs-server's CPU consumption always goes extraordinarily high, after running for sometime.
// 3. According to current server protocol design, the client should store what it's expecting, whether it's file or folder. 
// 4. Current server protocol does not transfer modified time or file size by default, need to think whether it should do so.
// 5. Fetching file (with versions and segments).
// 6. Current directory display.

// NDN communication functions
var face = new Face();
var prefix;
var currentDirectory;

function onData(interest, data)
{
  clearRows();
  
  var content = new Uint8Array(data.getContent().size());
  content.set(data.getContent().buf());
  var dirlist = DirProto.DirInfoArray.decode(content.buffer);
  
  for (var i = 0; i < dirlist.di.length; i++) {
	var path = { name: "", directory: "" };
	separateName(dirlist.di[i].path, path);
	
	var urlName = new Name(prefix);
	if (path.directory != "") {
	  urlName.append(path.directory)
	}
	
	if (dirlist.di[i].type === 1) {
	  // type '1' is file
	  addRow(path.name, urlName.append(path.name), 0, "N/A", "N/A");
	}
	else {
	  // type '2' is folder
	  addRow(path.name, urlName.append(path.name), 1, "N/A", "N/A");
	} 
  }
}

function onTimeout(interest)
{
  console.log("onTimeout called: " + interest.getName().toUri());
  console.log("Host: " + face.connectionInfo.toString());
}

function start() 
{
  prefix = new Name(document.getElementById('prefix').value);
  face.expressInterest(prefix, onData, onTimeout);
}

// (name)name	: ndn request name
function browse(name) 
{
  console.log(name);
  console.log("click called for name" + name.toUri());
  face.expressInterest(name, onData, onTimeout);
  return false;
}

/**
 * Folder browsing functions copied from viewing folder in Chrome
 */
// (string)name	: string which should be separated into directory + name
function separateName(fullpath, path) 
{
  // this assumes the string begins with '/' and ends without '/'
  // which is always the case for ndnfs
  var i = fullpath.lastIndexOf('/');
  path.name = fullpath.substr(i + 1);
  if (i > 0) {
    path.directory = fullpath.substr(1, i - 1);
  }
  else {
    path.directory = "";
  }
  console.log(path.directory);
}

// (string)name	: displayed name;
// (name)url	: ndn request name;
function addRow(name, url, isdir, size, dateModified) 
{
  if (name == ".")
    return;
  var table = document.getElementById("table");
  var row = document.createElement("tr");
  var fileCell = document.createElement("td");
  var link = document.createElement("a");

  link.className = isdir ? "icon dir" : "icon file";

  if (name == "..") {
    
  } else {
    if (isdir) {
      name = name + "/";
      size = "";
    }
    link.innerText = name;
    link.addEventListener('click', function () {
      browse(url);
    });
  }
  fileCell.appendChild(link);

  row.appendChild(fileCell);
  row.appendChild(createCell(size));
  row.appendChild(createCell(dateModified));

  table.appendChild(row);
}

function clearRows() 
{
  var table = document.getElementById("table");
  while (table.firstChild) {
	table.removeChild(table.firstChild);
  }
}

function createCell(text) 
{
  var cell = document.createElement("td");
  cell.setAttribute("class", "detailsColumn");
  cell.innerText = text;
  return cell;
}

function onListingParsingError() 
{
  var box = document.getElementById("listingParsingErrorBox");
  box.innerHTML = box.innerHTML.replace("LOCATION", encodeURI(document.location)
      + "?raw");
  box.style.display = "block";
}