npx babel-upgrade --write
npm install babel-loader@next
npx babel-upgrade --write --install

npm install --save-dev @babel/preset-typescript
npm install --save-dev @babel/preset-typescript @babel/preset-env @babel/plugin-proposal-class-properties @babel/plugin-proposal-object-rest-spread
npm install --save-dev babel-core
npm install --save-dev @babel/core
npm install --save-dev @babel/preset-es2015
npm install --save-dev @babel/preset-env


npm install -g @babel/preset-typescript @babel/preset-env @babel/plugin-proposal-class-properties @babel/plugin-proposal-object-rest-spread
npm install -g babel-core
npm install -g @babel/core
npm install -g @babel/preset-es2015
npm install -g @babel/preset-env

mkdir script_compilation
cd script_compilation
npm init

echo "./node_modules/.bin/babel.cmd" test_transpile.js --out-file test_transpile.js.ts seems to work