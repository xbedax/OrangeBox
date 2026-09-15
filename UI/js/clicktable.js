/*
 * Click table - clickable rows
 *
 */

// Config

// overit ze to neni potreba a smazat
function addRowHandlersOld() {
  const tableHead = document.getElementById("pintable").tHead;
  let idarray = [];
  for (let hi = 0; hi < getElementsByTagName("th").length; hi++){
    idarray[hi] = tableHead.getElementsByTagName("th")[ri].name;
  }

  var table = document.getElementById("pinTable");
  var rows = table.getElementsByTagName("tr");
  for (i = 1; i < rows.length; i++) {
    var currentRow = table.rows[i];
    var createClickHandler = 
        function(row) 
           {
                return function() { 
                                      let rowCont = {};
                                      for (let ri = 0; ri <= rows.length; ri++) {
                                        rowCont[idarray[ri]] = row.getElementsByTagName("td")[ri].innerHTML;
                                      }  
                                      toggleModal("modal-pin", rowCont);
                                 };
            };

        currentRow.onclick = createClickHandler(currentRow);
    }
}

function addRowHandlers() {
  const tableHead = document.getElementById("pinTableHead");
  let idarray = [];
  let headers = tableHead.getElementsByTagName("th");
  for (let hi = 0; hi < headers.length; hi++){
    idarray[hi] = headers[hi].getAttribute("name");
  }
 

    document.getElementById("pinrows").onclick = function(e) {
        let rowCont = {};
        let row = e.target.closest("tr");
        if (!row) return;

        let cells = row.getElementsByTagName("td");
        

        for (let i = 0; i < headers.length; i++) {
           rowCont[idarray[i]] = cells[i].innerHTML;
        }

        toggleModal("modal-pin", rowCont);
    };
}

//window.onload = addRowHandlers();