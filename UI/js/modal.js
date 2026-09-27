/*
 * Modal
 *
 * OrangeBox - https://box.inforoom.cz
 * Copyright 2025-2026 - Licensed under MIT
 */

// Config
const isOpenClass = "modal-is-open";
const openingClass = "modal-is-opening";
const closingClass = "modal-is-closing";
const animationDuration = 400; // ms
let visibleModal = null;

// Keep wire dates separate from the empty value used by native date pickers.
const credDateBounds = Object.freeze({
  datefrom: '0001-01-01', codefrom: '0001-01-01',
  dateto: '9999-12-31', codeto: '9999-12-31'
});

const toggleModal = (targetId, rowData = null) => {

    console.log("ToggleModal invoked on " + targetId);
    const modal = document.getElementById(targetId);
//    modal.open = true;
  typeof modal != "undefined" && modal != null && isModalOpen(modal)
    ? closeModal(modal)
    : openModal(modal, rowData);
};

// Toggle modal — submit variant: collect form then close
const submitModal = (event) => {
  const clickedElement = event.currentTarget.dataset.action || event.currentTarget.id;
  event.preventDefault();
  const modal = document.getElementById(event.currentTarget.getAttribute("data-target"));
  if (typeof modal == "undefined" || modal == null) return;
  console.log ("EventData: " + event);
  const formData = collectForm(modal);
  if (formData !== null) {
    let mycommand = formData["_command_"];
    delete formData._command_;
    formData["clicked"] = clickedElement;
    console.log("ClickedElement:" + clickedElement);
    postFormData(formData, mycommand);
  }
  closeModal(modal);
};

// Is modal open
const isModalOpen = (modal) => {
  return modal.hasAttribute("open") && modal.getAttribute("open") != "false" ? true : false;
};

// Open modal
const openModal = (modal, rowData ) => {
  if (isScrollbarVisible()) {
    document.documentElement.style.setProperty("--scrollbar-width", `${getScrollbarWidth()}px`);
  }
  document.documentElement.classList.add(isOpenClass, openingClass);
  setTimeout(() => {
    visibleModal = modal;
    document.documentElement.classList.remove(openingClass);
  }, animationDuration);
  modal.setAttribute("open", true);
  const form = modal.querySelector("form");
  const deleteButton = modal.querySelector('[data-action="deletebutton"]');
  if (rowData == null) {
    form.reset();
	if(modal.id == "modal-code") {
		document.getElementById("codeid").value = "0";
		document.getElementById("codevalue").disabled=false;
		document.getElementById("codename").disabled=false;
	}
	if(modal.id == "modal-pin") {
		document.getElementById("pinid").value = "0";
		document.getElementById("pinvalue").disabled=false;
		document.getElementById("btnpropose").disabled=false;
		document.getElementById("pinname").disabled=false;
        form.elements.namedItem("amount").disabled = form.elements.namedItem("checkbox-unlimited").checked;
	}
	
    //document.getElementById("pinid").value = "0";
    if (deleteButton) deleteButton.disabled = true;
    //document.getElementById("btnpropose").disabled=false;
    //document.getElementById("pinname").disabled=false;
    //document.getElementById("pinvalue").disabled=false;
  } else {
    fillModal(modal, rowData);  
	if(modal.id == "modal-code") {
		document.getElementById("codevalue").disabled=true;
		document.getElementById("codename").disabled=true;
	}
	if(modal.id == "modal-pin") {
		document.getElementById("pinvalue").disabled=true;
		document.getElementById("btnpropose").disabled=true;
		document.getElementById("pinname").disabled=true;
	}
    //document.getElementById("pinname").disabled=true;
    //document.getElementById("pinvalue").disabled=true;  
    if (deleteButton) deleteButton.disabled = false;
    //document.getElementById("btnpropose").disabled=true;    
  }
};

// Close modal
const closeModal = (modal) => {
  visibleModal = null;
  document.documentElement.classList.add(closingClass);
  setTimeout(() => {
    document.documentElement.classList.remove(closingClass, isOpenClass);
    document.documentElement.style.removeProperty("--scrollbar-width");
    modal.removeAttribute("open");
  }, animationDuration);
};

 // Close with a click outside
document.addEventListener("click", (event) => {
  if (visibleModal != null) {
    const modalContent = visibleModal.querySelector("article");
    const isClickInside = modalContent.contains(event.target);
    !isClickInside && closeModal(visibleModal);
  }
});

// Close with Esc key
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && visibleModal != null) {
    closeModal(visibleModal);
  }
});

// Fill cred detail modal form 
function fillModal(targetModal, fdata) {
  const form = targetModal.querySelector("form");
  for (const [key, value] of Object.entries(fdata)) {
    const field = form.elements.namedItem(key);
    if (field) field.value = credDateBounds[key] === value ? '' : value;
  }
  if (targetModal.id === "modal-pin") {
    const unlimited = String(fdata.amount) === "-1";
    form.elements.namedItem("amount").disabled = unlimited;
    form.elements.namedItem("checkbox-unlimited").checked = unlimited;
  }
}

// Get scrollbar width
const getScrollbarWidth = () => {
  // Creating invisible container
  const outer = document.createElement("div");
  outer.style.visibility = "hidden";
  outer.style.overflow = "scroll"; // forcing scrollbar to appear
  outer.style.msOverflowStyle = "scrollbar"; // needed for WinJS apps
  document.body.appendChild(outer);

  // Creating inner element and placing it in the container
  const inner = document.createElement("div");
  outer.appendChild(inner);

  // Calculating difference between container's full width and the child width
  const scrollbarWidth = outer.offsetWidth - inner.offsetWidth;

  // Removing temporary elements from the DOM
  outer.parentNode.removeChild(outer);

  return scrollbarWidth;
};

// Is scrollbar visible
const isScrollbarVisible = () => {
  return document.body.scrollHeight > screen.height;
};

// Collect form fields from the modal into a plain { id: value } object.
// Fields without an id are keyed by their name attribute; nameless ones are skipped.
// Returns null when no form is found in the modal.
const collectForm = (modal) => {
  const form = modal.querySelector("form");
  if (!form) return null;
  const data = {};
  for (const el of form.elements) {
    // Skip buttons, fieldsets, and anything without a usable key
    if (el.tagName === "BUTTON" || el.tagName === "FIELDSET") continue;
    const key = el.id || el.name;
    if (!key) continue;
 
    if (el.type === "checkbox" || el.type === "radio") {
      data[key] = el.checked;
    } else if (el.type === "select-multiple") {
      data[key] = Array.from(el.selectedOptions).map((o) => o.value);
    } else if (Object.hasOwn(credDateBounds, key)) {
      data[key] = el.value || credDateBounds[key];
    } else {
      data[key] = el.value;
    }
  }
  return data;
};
 
