mkdir script_compile
cd script_compile
npm init

npm install --save-dev @babel/core @babel/cli

npx babel-upgrade --write
npm install --save-dev @babel/preset-typescript
npm install --save-dev @babel/preset-env