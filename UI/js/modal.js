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
  const clickedElement = event.currentTarget.id;
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
  if (rowData == null) {
    form.reset();
    document.getElementById("pinid").value = "0";
    document.getElementById("deletebutton").disabled=true;
    document.getElementById("btnpropose").disabled=false;
    document.getElementById("pinname").disabled=false;
    document.getElementById("pinvalue").disabled=false;
  } else {
    fillModal(modal, rowData);  
    document.getElementById("pinname").disabled=true;
    document.getElementById("pinvalue").disabled=true;  
    document.getElementById("deletebutton").disabled=false;  
    document.getElementById("btnpropose").disabled=true;    
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

// Fill pin detail modal form 
function fillModal(targetModal, fdata) {
//  var fkeys = Object.keys(fdata);
  for (var key in  fdata){
        console.log("Filling key: " + key + "=" + fdata[key]);
//      let tmodal = document.getElementById(targetModal);
      document .getElementById(key).value = fdata[key];
  }
  
  if(fdata["amount"] == "-1") {
    document .getElementById("amount").disabled=true;
    document .getElementById("checkbox-unlimited").checked=true;
  }else{
    document .getElementById("checkbox-unlimited").checked=false;
    document .getElementById("amount").disabled=false;
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
    } else {
      data[key] = el.value;
    }
  }
  return data;
};
 
