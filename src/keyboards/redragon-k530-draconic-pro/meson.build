kb_dir = meson.current_source_dir()

keyboards += {
    'redragon-k530-draconic-pro' : {
        'dir' : kb_dir,
        'platform' : 'sh68f90',
        'src' : files(
            'user_matrix.c',
            'user_init.c',
            'user_sleep.c',
            'kb.c',
        ),
        'layouts' : {
            'default' : {
                'dir' : kb_dir / 'layouts/default',
                'src' : files(
                    'layouts/default/indicators.c',
                    'layouts/default/layout.c',
                ),
            },
        },
        'options' : {
            'vendor_id' : '0x258a',
            'product_id' : '0x0049',
            'wireless' : 'bk3632',
            'manufacturer' : 'Redragon',
            'product' : 'K530 Draconic Pro',
        },
    },
}
