// Runs against the actual UI HTML and scripts with a mocked WebSocket transport.
try {
  function check(condition, message) { if (!condition) throw new Error(message); }
  const codeRows = {
    codeid: [2], codename: ['<Tleskac>'], codevalue: ['HZ1268956754M'],
    codefrom: ['0001-01-01'], codeto: ['9999-12-31']
  };
  handleMessage({_command_: 'c_setcred', data: {credType: 'ctCode', rows: codeRows, rowCount: 1}});
  check(document.querySelector('#coderows tr').cells[1].textContent === '<Tleskac>', 'escaped code name');
  check(!document.querySelector('#coderows tleskac'), 'name must not become markup');
  check(document.querySelector('#coderows tr').cells[3].textContent === 'Neomezeno', 'unbounded start label');
  check(document.querySelector('#coderows tr').cells[4].textContent === 'Neomezeno', 'unbounded end label');
  document.querySelector('#coderows td').click();
  const codeModal = document.getElementById('modal-code');
  check(document.getElementById('codeid').value === '2', 'row fills code id');
  check(document.getElementById('codename').value === '<Tleskac>', 'row click preserves text');
  check(document.getElementById('code-deletebutton').disabled === false, 'code delete enabled');
  check(document.getElementById('codevalue').disabled, 'existing identity immutable');
  check(document.getElementById('codefrom').value === '', 'start picker has no year 1');
  check(document.getElementById('codeto').value === '', 'end picker has no year 9999');
  document.getElementById('code-savebutton').click();
  let sent = testMessages.at(-1);
  check(sent.command === 'set_cred' && sent.data.credType === 'ctCode', 'code save command and type');
  check(sent.data.clicked === 'savebutton' && sent.data.codeid === '2', 'code save action and id');
  check(sent.data.codevalue === 'HZ1268956754M', 'disabled identity is sent');
  check(sent.data.codefrom === '0001-01-01' && sent.data.codeto === '9999-12-31', 'unchanged unbounded dates round-trip');
  document.getElementById('codeto').value = '2027-05-20';
  check(collectForm(codeModal).codeto === '2027-05-20', 'selected date limits validity');
  document.getElementById('codeto').value = '';
  check(collectForm(codeModal).codeto === '9999-12-31', 'clearing date restores unlimited validity');
  document.getElementById('code-deletebutton').click();
  check(testMessages.at(-1).data.clicked === 'deletebutton', 'code delete action');
  openModal(codeModal, null);
  check(document.getElementById('codeid').value === '0', 'new code id');
  check(document.getElementById('code-deletebutton').disabled, 'new code cannot be deleted');

  handleMessage({_command_: 'c_setcred', data: {credType: 'ctPin', rowCount: 1, rows: {
    pinid: [1], pinname: ['Kule'], pinvalue: ['49786543'],
    datefrom: ['2000-02-10'], dateto: ['2099-04-01'], amount: ['3']
  }}});
  document.querySelector('#pinrows td').click();
  check(document.getElementById('amount').value === '3', 'password amount');
  check(document.getElementById('checkbox-unlimited').checked === false, 'password limited count');
  document.getElementById('savebutton').click();
  sent = testMessages.at(-1);
  check(sent.command === 'set_cred' && sent.data.credType === 'ctPin', 'password save command and type');
  check(sent.data.pinid === '1' && sent.data.clicked === 'savebutton', 'password id and action');
  check(sent.data.datefrom === '2000-02-10' && sent.data.dateto === '2099-04-01', 'finite password dates preserved');
  fillModal(document.getElementById('modal-pin'), {datefrom: '0001-01-01', dateto: '9999-12-31', amount: '3'});
  check(document.getElementById('datefrom').value === '' && document.getElementById('dateto').value === '', 'password unbounded pickers empty');
  check(collectForm(document.getElementById('modal-pin')).dateto === '9999-12-31', 'password upper bound round-trip');
  openModal(document.getElementById('modal-pin'), null);
  check(document.getElementById('deletebutton').disabled, 'new password cannot be deleted');
  check(document.getElementById('amount').disabled === document.getElementById('checkbox-unlimited').checked,
    'new password resets count controls');

  getCreds(7, 5, 2);
  handleMessage({_command_: 'c_setcred', data: {credType: 'ctCode', changed: true}});
  sent = testMessages.at(-1);
  check(sent.command === 'get_creds' && sent.data.credId === 7 && sent.data.credCount === 5,
    'notification preserves pagination');
  const before = document.getElementById('coderows').innerHTML;
  handleMessage({_command_: 'c_setcred', data: {credType: 'ctCode', success: false, error: 'invalid_date_range', lastresult: 'invalid_date_range'}});
  check(document.getElementById('coderows').innerHTML === before, 'error preserves rows');
  check(document.getElementById('lastresult').textContent === 'invalid_date_range', 'error shown');
  handleMessage({_command_: 'c_setcred', data: {credType: 'ctCode', rowCount: 0,
    rows: {codeid: [], codename: [], codevalue: [], codefrom: [], codeto: []}}});
  check(document.querySelectorAll('#coderows tr').length === 0, 'empty response clears table');
  document.body.dataset.testResult = 'PASS: credential UI rows, dialogs, actions, errors and refresh';
} catch (error) {
  document.body.dataset.testResult = 'FAIL: ' + error.message;
}
