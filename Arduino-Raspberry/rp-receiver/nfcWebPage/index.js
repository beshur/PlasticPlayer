/*
 * Client-side app for adding and editing NFC records
 */
class App {
  constructor() {
    this.version = '1.0.0';
    this.state = new AppState();
    this.view = new AppView({
      el: document.getElementById('app'),
      state: this.state
    });
  }

  start() {
    this.state.load()
      .then(result => {
        this.view.render();
      })
      .catch(err => {
        this.view.onError(err);
      });
  }
}


class AppState {
  constructor() {
    this.records = [];
  }

  getRecords() {
    return this.records;
  }

  dummyRecord() {
    return {
      id: '',
      link: '',
      description: ''
    }
  }

  load() {
    this.records = [
      {
        id: '04:50:D3:FA:86:52:81',
        link: 'http://prem2.di.fm:80/deeptech_hi?30a30bbf39c900e4868d0f4a',
        description: 'Deep House'
      }
    ];

    return new Promise((resolve, reject) => {
      // #TODO call to server
      resolve(this.records);
    });
  }

  update(records) {
    this.records = records;
    this.save();
  }

  save() {
    // #TODO

  }
}

class AppView {
  constructor(options) {
    this.state = options.state;
    this.el = options.el;

    this.cards = this.el.querySelector('.cards');
    this.tbody = this.cards.querySelector('tbody');
    this.form = this.el.querySelector('form');
    this.cardsItems = [];

    this.onAdd = this.onAdd.bind(this);
    this.onSave = this.onSave.bind(this);

    this.el.addEventListener('click', (e) => {
      if(e.target) {
        if (e.target.classList.contains('remove')) {
          this.onRemove(e);
        }
        if (e.target.classList.contains('dismiss')) {
          this.onDismiss(e);
        }
      }
    });

    this.el.querySelector('#addRow').addEventListener('click', this.onAdd);

    this.el.querySelector('#save').addEventListener('click', this.onSave);
    this.form.addEventListener('submit', (e) => {
      e.preventDefault();
      this.onSave();
    });
  }

  onSave() {
    let data = this.serialize();
    this.state.update(data);
  }

  render() {
    let html = this.state.getRecords().map(this.tpl().row);
    this.tbody.innerHTML = html;
  }

  onAdd(e) {
    let tmp = document.createElement('table');
    tmp.innerHTML = this.tpl().row(this.state.dummyRecord());
    let theRow = tmp.firstChild.firstChild;
    this.tbody.appendChild(tmp.firstChild.firstChild);
    theRow.querySelector('input').focus();
    return this.el;
  }

  onRemove(e) {
    e.target.closest('tr').remove();
  }

  onDismiss(e) {
    e.target.closest('div').remove();
  }

  onError(e) {
    this.onMessage(e);
  }

  onMessage(message) {
    let messageView = document.createElement('div');
    messageView.innerHTML = this.tpl().message(message);
    this.el.querySelector('.messages').appendChild(messageView);
  }


  serialize() {
    let records = [];

    this.cards.querySelectorAll('tbody tr').forEach(row => {
      let inputs = row.querySelectorAll('input');
      let item = this.state.dummyRecord();
      item.id = inputs[0].value;
      item.link = inputs[1].value;
      item.description = inputs[2].value;

      records.push(item);
    });

    console.log('serialize', records);

    return records;
  }

  tpl() {
    return {
      row: function(item) {
        return `<tr>
          <td><button class="remove" title="Remove row">&#10008;</button></td>
          <td><input type="text" value="${item.id}" /></td>
          <td><input type="text" value="${item.link}" /></td>
          <td><input type="text" value="${item.description}" /></td>
        </tr>`;
      },
      message: function(text) {
        return `<span class="dismiss" alt="dismiss">&#10008;</span>${text}`
      }
    }
  }
}

class AppRowView {
  constructor(options) {
    this.options = options;

    this.options.el.querySelector('.remove').addEventListener('click', this.onRemove);
  }

  onRemove() {
    this.options.model
  }
}


window.onload = function() {
  const appInstance = new App();
  appInstance.start();
}
