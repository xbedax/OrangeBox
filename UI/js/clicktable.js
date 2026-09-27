/*
 * Click table - clickable rows
 *
 */

// Config

function addRowHandlers(type) {
  const tableHead = document.getElementById(type + "TableHead");
  let idarray = [];
  let headers = tableHead.getElementsByTagName("th");
  for (let hi = 0; hi < headers.length; hi++){
    idarray[hi] = headers[hi].getAttribute("name");
  }
 

    document.getElementById(type + "rows").onclick = function(e) {
        let rowCont = {};
        let row = e.target.closest("tr");
        if (!row) return;

        let cells = row.getElementsByTagName("td");
        

        for (let i = 0; i < headers.length; i++) {
           rowCont[idarray[i]] = cells[i].dataset.credValue ?? cells[i].textContent;
        }

        toggleModal("modal-" + type, rowCont);
    };
}

//window.onload = addRowHandlers();
