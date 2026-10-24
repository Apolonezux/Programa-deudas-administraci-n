(() => {
    const status = document.getElementById("app-status");
    const clientsBody = document.getElementById("clients-table");
    const paymentsBody = document.getElementById("payments-table");
    const clientSelect = document.getElementById("cliente");
    const clientFilter = document.getElementById("client-filter");
    const paymentFilter = document.getElementById("payment-filter");
    const clientForm = document.querySelector('form[action="/cliente"]');
    const paymentForm = document.getElementById("payment-form");
    const money = new Intl.NumberFormat("es-MX", {
        style: "currency",
        currency: "MXN"
    });
    let clients = [];
    let payments = [];

    function setStatus(message, state) {
        status.textContent = message;
        status.dataset.state = state || "ok";
    }

    function localDate() {
        const now = new Date();
        const month = String(now.getMonth() + 1).padStart(2, "0");
        const day = String(now.getDate()).padStart(2, "0");
        return `${now.getFullYear()}-${month}-${day}`;
    }

    async function loadData() {
        const responses = await Promise.all([
            fetch("/api/clientes"),
            fetch("/api/pagos")
        ]);
        for (const response of responses) {
            if (!response.ok) {
                throw new Error(`Error al consultar datos (${response.status}).`);
            }
        }
        [clients, payments] = await Promise.all(responses.map(response => response.json()));
        render();
        setStatus("Datos actualizados.");
    }

    function addCell(row, value, className) {
        const cell = document.createElement("td");
        cell.textContent = value;
        if (className) {
            cell.className = className;
        }
        row.append(cell);
    }

    function renderClients() {
        const search = clientFilter.value.trim().toLocaleLowerCase();
        const paidByClient = new Map();
        for (const payment of payments) {
            paidByClient.set(payment.name,
                (paidByClient.get(payment.name) || 0) + Number(payment.amount));
        }
        clientsBody.replaceChildren();
        const visibleClients = clients.filter(client =>
            [client.name, client.contact, client.lot]
                .some(value => value.toLocaleLowerCase().includes(search))
        );

        if (visibleClients.length === 0) {
            const row = clientsBody.insertRow();
            const cell = row.insertCell();
            cell.colSpan = 6;
            cell.textContent = clients.length ? "No hay resultados." : "Aún no hay clientes registrados.";
        }

        for (const client of visibleClients) {
            const paid = paidByClient.get(client.name) || 0;
            const balance = Number(client.initialBalance) - paid;
            const row = clientsBody.insertRow();
            addCell(row, client.name);
            addCell(row, client.contact);
            addCell(row, client.lot);
            addCell(row, money.format(Number(client.initialBalance)));
            addCell(row, money.format(paid));
            addCell(row, balance < 0
                ? `Saldo a favor: ${money.format(Math.abs(balance))}`
                : money.format(balance), balance < 0 ? "balance-credit" : "");
        }

        document.getElementById("total-clients").textContent = clients.length;
        const totalBalance = clients.reduce((total, client) =>
            total + Number(client.initialBalance) - (paidByClient.get(client.name) || 0), 0);
        document.getElementById("total-balance").textContent = money.format(totalBalance);

        const currentSelection = clientSelect.value;
        clientSelect.replaceChildren(new Option("Selecciona un cliente", ""));
        for (const client of clients) {
            clientSelect.add(new Option(client.name, client.name));
        }
        if (clients.some(client => client.name === currentSelection)) {
            clientSelect.value = currentSelection;
        }
        clientSelect.disabled = clients.length === 0;
    }

    function renderPayments() {
        const search = paymentFilter.value.trim().toLocaleLowerCase();
        const visiblePayments = payments.filter(payment =>
            payment.name.toLocaleLowerCase().includes(search) ||
            payment.folio.toLocaleLowerCase().includes(search)
        );
        paymentsBody.replaceChildren();
        if (visiblePayments.length === 0) {
            const row = paymentsBody.insertRow();
            const cell = row.insertCell();
            cell.colSpan = 4;
            cell.textContent = payments.length ? "No hay resultados." : "Aún no hay pagos registrados.";
        }
        for (const payment of visiblePayments.slice().reverse()) {
            const row = paymentsBody.insertRow();
            addCell(row, payment.date);
            addCell(row, payment.name);
            addCell(row, money.format(Number(payment.amount)));
            addCell(row, payment.folio);
        }
        document.getElementById("total-payments").textContent = payments.length;
    }

    function render() {
        renderClients();
        renderPayments();
    }

    async function submitForm(event) {
        event.preventDefault();
        const form = event.currentTarget;
        const button = form.querySelector('button[type="submit"]');
        button.disabled = true;
        setStatus("Guardando...");
        try {
            const response = await fetch(form.action, {
                method: "POST",
                body: new URLSearchParams(new FormData(form))
            });
            if (!response.ok) {
                if (response.status === 409) {
                    throw new Error(await response.text());
                }
                throw new Error(response.status === 400
                    ? "Revisa los datos: puede faltar un campo o ser inválido."
                    : `No se pudo guardar el registro (error ${response.status}).`);
            }
            form.reset();
            if (form === paymentForm) {
                document.getElementById("fecha").value = localDate();
            }
            await loadData();
            setStatus("Registro guardado correctamente.");
        } catch (error) {
            setStatus(error.message || "No se pudo conectar con el servidor.", "error");
        } finally {
            button.disabled = false;
        }
    }

    clientFilter.addEventListener("input", renderClients);
    paymentFilter.addEventListener("input", renderPayments);
    clientForm.addEventListener("submit", submitForm);
    paymentForm.addEventListener("submit", submitForm);
    document.getElementById("fecha").value = localDate();
    loadData().catch(error => setStatus(error.message || "No se pudieron cargar los datos.", "error"));
})();
