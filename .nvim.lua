
vim.api.nvim_create_autocmd("FileType", {
    pattern = { "c", "cpp" },
    callback = function()
        -- Usamos :split para abrir una ventana abajo y :term para la terminal
        vim.keymap.set('n', '<leader>r', '<cmd>split | term ninja -C _build && ./_build/gtkdeck<cr>', {
            buffer = 0,
            desc = "Build and Run GTK"
        })
    end,
})
